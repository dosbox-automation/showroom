// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/install_runner.h"

#include "app/logging.h"
#include "engine/conf_writer.h"
#include "model/install_check.h"
#include "net/download_plan.h"

#include <QFile>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>
#include <system_error>
#include <utility>

namespace showroom {
namespace {

constexpr const char* kLogComponent = "install_runner";

std::optional<int> progressFromOutput(const QJsonObject& output)
{
    // The engine delivers progress as a number even when the recipe
    // assigns a string, so both arrive here.
    const QJsonValue value = output.value("progress");
    if (value.isDouble()) {
        return static_cast<int>(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

// A self-extractor is a DOS program the host cannot unpack; it runs
// inside the machine, so preparing the extracts dir is a copy with the
// installer's timestamp kept.
ExtractResult copySelfExtractor(const std::filesystem::path& exe,
                                const std::filesystem::path& destination)
{
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return {false, "cannot create extracts dir: " + ec.message()};
    }
    const auto stamp = std::filesystem::last_write_time(exe, ec);
    if (ec) {
        return {false, "cannot read installer timestamp: " + ec.message()};
    }
    std::filesystem::copy_file(exe,
                               destination,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        return {false, "cannot copy installer: " + ec.message()};
    }
    std::filesystem::last_write_time(destination, stamp, ec);
    if (ec) {
        return {false, "cannot restore installer timestamp: " + ec.message()};
    }
    return {true, ""};
}

// The showroom's own config artifacts (a game's known-good sound
// config, captured once in a probe) land on top of the extracted
// files. Missing overlay dir is the common case and a no-op.
bool copyOverlay(const std::filesystem::path& overlay,
                 const std::filesystem::path& staging, std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(overlay, ec)) {
        return true;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(overlay, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         ++it) {
        const auto target = staging / it->path().lexically_relative(overlay);
        if (it->is_directory()) {
            std::filesystem::create_directories(target, ec);
        } else if (it->is_regular_file()) {
            const auto stamp = std::filesystem::last_write_time(it->path(), ec);
            std::filesystem::create_directories(target.parent_path(), ec);
            std::filesystem::copy_file(it->path(),
                                       target,
                                       std::filesystem::copy_options::overwrite_existing,
                                       ec);
            if (!ec) {
                std::filesystem::last_write_time(target, stamp, ec);
            }
        } else {
            error = "overlay entry is neither file nor directory: " + it->path().string();
            return false;
        }
        if (ec) {
            error = "overlay copy failed at " + it->path().string() + ": " + ec.message();
            return false;
        }
    }
    if (ec) {
        error = "overlay walk failed: " + ec.message();
        return false;
    }
    return true;
}

} // namespace

InstallRunner::InstallRunner(std::filesystem::path engine_binary,
                             std::filesystem::path cache_base,
                             std::filesystem::path games_dir,
                             const ArchiveExtractor& extractor, quint16 engine_port,
                             QObject* parent)
        : QObject(parent),
          engine_binary_(std::move(engine_binary)),
          cache_base_(std::move(cache_base)),
          games_dir_(std::move(games_dir)),
          extractor_(extractor),
          engine_port_(engine_port),
          engine_(engine_binary_)
{
    poll_timer_.setInterval(kPollIntervalMs);
    deadline_timer_.setSingleShot(true);
    connect(&poll_timer_, &QTimer::timeout, this, &InstallRunner::onPollTick);
    connect(&deadline_timer_, &QTimer::timeout, this, [this]() {
        log_warn(kLogComponent, "%s: install deadline elapsed", qPrintable(slug_));
        stopEngineThen("install timed out");
    });
    connect(&engine_, &EngineProcess::finished, this, &InstallRunner::onEngineEnded);
    connect(&engine_, &EngineProcess::failed, this, [this](const QString& reason) {
        failInstall("engine failed to start: " + reason);
    });
}

InstallRunner::~InstallRunner()
{
    // The members this object's engine-signal handlers touch are destroyed
    // before the engine member is; a signal emitted while the engine kills
    // its child in its own destructor must not reach them.
    disconnect(&engine_, nullptr, this, nullptr);
}

bool InstallRunner::startInstall(const GameDefinition& game, std::string& error)
{
    error.clear();
    if (phase_ != Phase::Idle || engine_.isRunning()) {
        error = "an install is already running";
        return false;
    }
    if (!isSafeSlug(game.slug())) {
        error = "slug \"" + game.slug() + "\" is not a safe directory name";
        return false;
    }
    if (game.install().max_runtime_seconds <= 0) {
        error = "game \"" + game.slug() + "\" declares no install runtime ceiling";
        return false;
    }

    if (downloadPlansFor(game).empty()) {
        error = "game \"" + game.slug() + "\" has no usable download source";
        return false;
    }
    // Whichever source's archive actually landed decides the filename
    // and the install type; the mirror's may differ from the primary's.
    const auto plan = archivePlanOnDisk(game,
                                        cache_base_ / "downloads" / game.slug());
    if (!plan) {
        error = "downloaded archive not found for \"" + game.slug() + "\"";
        return false;
    }
    archive_ = cache_base_ / "downloads" / game.slug() / plan->filename;
    std::error_code ec;

    const auto extracts_base = cache_base_ / "extracts";
    extracts_dir_ = ConfWriter::extractsDir(cache_base_, game.slug());
    if (!isWithin(extracts_base, extracts_dir_)) {
        error = "extracts dir escapes the cache";
        return false;
    }
    staging_dir_ = ConfWriter::installStagingDir(extracts_dir_);
    if (!isWithin(extracts_dir_, staging_dir_)) {
        error = "staging dir escapes the extracts dir";
        return false;
    }

    if (plan->install_type == InstallType::Unzip) {
        // Extraction is the whole install: no engine, no recipe. The
        // archive unpacks straight into staging, then the shared
        // verify-and-promote path takes over on the next loop turn.
        std::filesystem::remove_all(staging_dir_, ec);
        if (!std::filesystem::create_directories(staging_dir_, ec) || ec) {
            error = "cannot create staging dir: " + staging_dir_.string();
            return false;
        }
        game_ = game;
        slug_ = QString::fromStdString(game.slug());
        phase_ = Phase::Direct;
        pending_failure_.clear();
        QTimer::singleShot(0, this, &InstallRunner::runDirectInstall);
        log_info(kLogComponent, "%s: direct install started", qPrintable(slug_));
        return true;
    }

    const auto recipe_path = games_dir_ / game.slug() / "recipe.lua";
    QFile recipe_file(QString::fromStdString(recipe_path.string()));
    if (!recipe_file.open(QIODevice::ReadOnly)) {
        error = "no install recipe: " + recipe_path.string();
        return false;
    }
    recipe_ = recipe_file.readAll();
    if (recipe_.isEmpty()) {
        error = "install recipe is empty: " + recipe_path.string();
        return false;
    }

    // A download that is the medium mounts from downloads/ in place, so
    // there is nothing to extract - and handing a disk image to the
    // extractor fails outright (aug-p0kd).
    if (!ConfWriter::downloadIsMedium(game, cache_base_)) {
        const bool extracted = std::filesystem::is_directory(extracts_dir_, ec)
                            && std::filesystem::directory_iterator(extracts_dir_, ec)
                                       != std::filesystem::directory_iterator();
        if (!extracted) {
            const auto result = game.sources().front().install_type
                                             == InstallType::ExeInstall
                                      ? copySelfExtractor(archive_,
                                                          extracts_dir_ / plan->filename)
                                      : extractor_.extract(archive_, extracts_dir_);
            if (!result.ok) {
                // A partial extraction would pass the non-empty check next
                // time and skip extraction against broken contents.
                std::filesystem::remove_all(extracts_dir_, ec);
                error = "extraction failed: " + result.error;
                return false;
            }
        }
    }

    std::filesystem::remove_all(staging_dir_, ec);
    if (!std::filesystem::create_directories(staging_dir_, ec) || ec) {
        error = "cannot create staging dir: " + staging_dir_.string();
        return false;
    }

    const auto conf = ConfWriter::writeInstallConf(game, cache_base_, error);
    if (!conf) {
        return false;
    }
    if (!engine_.start(*conf, error)) {
        return false;
    }

    game_ = game;
    slug_ = QString::fromStdString(game.slug());
    api_ = std::make_unique<ApiClient>("127.0.0.1", engine_port_, engine_.token());
    engine_.setShutdownRequester([this]() {
        api_->requestShutdown([](const ApiClient::Response&) {});
        return true;
    });

    phase_ = Phase::Starting;
    pending_failure_.clear();
    last_progress_ = -1;
    poll_failures_ = 0;
    request_in_flight_ = false;
    // The runtime ceiling times the script; the boot budget covers the
    // engine's webserver coming up. Re-armed on the phase change.
    deadline_timer_.start(kEngineStartBudgetMs);
    poll_timer_.start();
    log_info(kLogComponent, "%s: install started", qPrintable(slug_));
    return true;
}

bool InstallRunner::isRunning() const
{
    return phase_ != Phase::Idle;
}

QString InstallRunner::installingSlug() const
{
    return phase_ == Phase::Idle ? QString() : slug_;
}

void InstallRunner::setStopTimeouts(int graceful_ms, int terminate_ms)
{
    engine_.setStopTimeouts(graceful_ms, terminate_ms);
}

void InstallRunner::runDirectInstall()
{
    if (phase_ != Phase::Direct) {
        return;
    }
    const auto result = extractor_.extract(archive_, staging_dir_);
    if (!result.ok) {
        failInstall("extraction failed: " + QString::fromStdString(result.error));
        return;
    }
    verifyAndPromote();
}

void InstallRunner::onPollTick()
{
    if (request_in_flight_) {
        return;
    }
    if (phase_ == Phase::Starting) {
        request_in_flight_ = true;
        api_->getStatus([this](const ApiClient::Response& response) {
            request_in_flight_ = false;
            // Refusals are expected while the webserver boots; the
            // deadline timer bounds the retries.
            if (response.ok && phase_ == Phase::Starting) {
                beginScript();
            }
        });
        return;
    }
    if (phase_ == Phase::Polling) {
        request_in_flight_ = true;
        api_->getScriptStatus([this](const ApiClient::Response& response) {
            request_in_flight_ = false;
            handleScriptStatus(response);
        });
    }
}

void InstallRunner::beginScript()
{
    poll_timer_.stop();
    request_in_flight_ = true;
    api_->loadScript(slug_ + "-install", recipe_, [this](const ApiClient::Response& r) {
        if (!r.ok) {
            request_in_flight_ = false;
            stopEngineThen("cannot load recipe: " + r.error);
            return;
        }
        api_->startScript([this](const ApiClient::Response& start) {
            request_in_flight_ = false;
            if (!start.ok) {
                stopEngineThen("cannot start recipe: " + start.error);
                return;
            }
            phase_ = Phase::Polling;
            deadline_timer_.start(game_->install().max_runtime_seconds * 1000);
            poll_timer_.start();
        });
    });
}

void InstallRunner::handleScriptStatus(const ApiClient::Response& response)
{
    if (phase_ != Phase::Polling) {
        return;
    }
    if (!response.ok) {
        ++poll_failures_;
        if (poll_failures_ >= kMaxConsecutivePollFailures) {
            stopEngineThen("status polling failed: " + response.error);
        }
        return;
    }
    poll_failures_ = 0;

    const QJsonObject output = response.body.value("output").toObject();
    if (const auto progress = progressFromOutput(output);
        progress.has_value() && *progress != last_progress_) {
        last_progress_ = *progress;
        emit progressChanged(slug_, std::clamp(*progress, 0, 100));
    }

    const QString state = response.body.value("state").toString();
    if (state == "error") {
        const QString detail = response.body.value("error").toString();
        stopEngineThen(detail.isEmpty() ? QString("recipe failed") : detail);
        return;
    }
    if (state == "completed") {
        if (output.value("install_complete").toString() == "yes") {
            stopEngineThen(QString());
        } else {
            stopEngineThen("recipe completed without signaling install_complete");
        }
    }
}

void InstallRunner::stopEngineThen(const QString& pending_failure)
{
    if (phase_ == Phase::Stopping || phase_ == Phase::Idle) {
        return;
    }
    phase_ = Phase::Stopping;
    pending_failure_ = pending_failure;
    poll_timer_.stop();
    deadline_timer_.stop();
    engine_.stop();
}

void InstallRunner::onEngineEnded(int exit_code)
{
    if (phase_ == Phase::Idle) {
        return;
    }
    if (phase_ != Phase::Stopping) {
        poll_timer_.stop();
        deadline_timer_.stop();
        failInstall(QString("engine ended unexpectedly (exit %1)").arg(exit_code));
        return;
    }
    if (!pending_failure_.isEmpty()) {
        failInstall(pending_failure_);
        return;
    }
    verifyAndPromote();
}

void InstallRunner::verifyAndPromote()
{
    std::string overlay_error;
    if (!copyOverlay(games_dir_ / game_->slug() / "overlay",
                     staging_dir_,
                     overlay_error)) {
        failInstall(QString::fromStdString(overlay_error));
        return;
    }
    const auto failures = verifyInstall(*game_, staging_dir_);
    if (!failures.empty()) {
        QStringList parts;
        for (const auto& failure : failures) {
            parts << QString::fromStdString(failure);
        }
        failInstall("verification failed: " + parts.join("; "));
        return;
    }

    const auto installs_base = cache_base_ / "installs";
    const auto target = installs_base / game_->slug();
    std::error_code ec;
    if (!isWithin(installs_base, target) || target == installs_base) {
        failInstall("install target escapes the cache");
        return;
    }
    std::filesystem::create_directories(installs_base, ec);
    // A damaged install being redone still sits at the target; the
    // verified staging result replaces it wholesale.
    std::filesystem::remove_all(target, ec);
    std::filesystem::rename(staging_dir_, target, ec);
    if (ec) {
        failInstall(QString("cannot move install into place: %1")
                            .arg(QString::fromStdString(ec.message())));
        return;
    }

    phase_ = Phase::Idle;
    log_info(kLogComponent, "%s: install verified and promoted", qPrintable(slug_));
    emit succeeded(slug_);
}

void InstallRunner::failInstall(const QString& reason)
{
    removeStaging();
    phase_ = Phase::Idle;
    log_warn(kLogComponent,
             "%s: install failed: %s",
             qPrintable(slug_),
             qPrintable(reason));
    emit failed(slug_, reason);
}

void InstallRunner::removeStaging()
{
    // Rollback deletes the staging dir alone: the archive, the extracted
    // images and any previous install at installs/<slug> all survive a
    // failed attempt.
    if (staging_dir_.empty() || !isWithin(extracts_dir_, staging_dir_)) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove_all(staging_dir_, ec);
}

} // namespace showroom
