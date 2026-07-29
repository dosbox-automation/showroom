// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/game_launcher.h"

#include "engine/conf_writer.h"

#include <QEventLoop>
#include <QTimer>

#include <system_error>
#include <utility>

namespace showroom {

GameLauncher::GameLauncher(std::filesystem::path engine_binary,
                           std::filesystem::path cache_base, quint16 engine_port,
                           QObject* parent)
        : QObject(parent),
          engine_binary_(std::move(engine_binary)),
          cache_base_(std::move(cache_base)),
          engine_port_(engine_port),
          engine_(engine_binary_)
{
    connect(&engine_, &EngineProcess::started, this, [this]() {
        emit gameStarted(running_slug_);
    });
    connect(&engine_, &EngineProcess::finished, this, [this](int) {
        const QString slug = running_slug_;
        running_slug_.clear();
        emit gameEnded(slug);
    });
    connect(&engine_, &EngineProcess::failed, this, [this](const QString& reason) {
        const QString slug = running_slug_;
        running_slug_.clear();
        emit launchFailed(slug, reason);
    });
}

GameLauncher::~GameLauncher()
{
    // The members this object's engine-signal handlers touch are destroyed
    // before the engine member is; a signal emitted while the engine kills
    // its child in its own destructor must not reach them.
    disconnect(&engine_, nullptr, this, nullptr);
}

bool GameLauncher::launch(const GameDefinition& game, std::string& error)
{
    error.clear();
    if (engine_.isRunning()) {
        error = "a game is already running";
        return false;
    }
    // Checked before the slug touches a path; ConfWriter checks it again.
    if (!isSafeSlug(game.slug())) {
        error = "slug \"" + game.slug() + "\" is not a safe directory name";
        return false;
    }
    std::error_code ec;
    const auto install_dir = cache_base_ / "installs" / game.slug();
    if (!std::filesystem::is_directory(install_dir, ec)) {
        error = "no install directory: " + install_dir.string();
        return false;
    }

    const auto conf = ConfWriter::writeConf(game, cache_base_, error);
    if (!conf) {
        return false;
    }
    if (!engine_.start(*conf, error)) {
        return false;
    }

    running_slug_ = QString::fromStdString(game.slug());
    api_ = std::make_unique<ApiClient>("127.0.0.1", engine_port_, engine_.token());
    engine_.setShutdownRequester([this]() {
        api_->requestShutdown([](const ApiClient::Response&) {});
        return true;
    });
    return true;
}

void GameLauncher::stop()
{
    engine_.stop();
}

bool GameLauncher::shutdownAndWait()
{
    if (!engine_.isRunning()) {
        return true;
    }
    // The ceiling exceeds the stop escalation on purpose: stop() already
    // ends in a kill, so the ceiling only guards the wait itself.
    QEventLoop loop;
    QTimer ceiling;
    ceiling.setSingleShot(true);
    connect(&engine_, &EngineProcess::finished, &loop, [&loop](int) { loop.quit(); });
    connect(&ceiling, &QTimer::timeout, &loop, &QEventLoop::quit);
    ceiling.start(engine_.stopEscalationBudgetMs() + kShutdownMarginMs);
    engine_.stop();
    loop.exec();
    return !engine_.isRunning();
}

bool GameLauncher::isRunning() const
{
    return engine_.isRunning();
}

QString GameLauncher::runningSlug() const
{
    return running_slug_;
}

void GameLauncher::setStopTimeouts(int graceful_ms, int terminate_ms)
{
    engine_.setStopTimeouts(graceful_ms, terminate_ms);
}

} // namespace showroom
