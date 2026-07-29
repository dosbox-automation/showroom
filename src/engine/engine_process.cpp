// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/engine_process.h"

#include <QProcessEnvironment>
#include <QRandomGenerator>

#include <array>
#include <system_error>
#include <utility>

namespace showroom {

EngineProcess::EngineProcess(std::filesystem::path engine_binary, QObject* parent)
        : QObject(parent),
          engine_binary_(std::move(engine_binary))
{
    graceful_timer_.setSingleShot(true);
    terminate_timer_.setSingleShot(true);

    connect(&process_, &QProcess::started, this, &EngineProcess::started);
    connect(&process_,
            &QProcess::finished,
            this,
            [this](int exit_code, QProcess::ExitStatus exit_status) {
                graceful_timer_.stop();
                terminate_timer_.stop();
                stopping_ = false;
                emit finished(exit_status == QProcess::NormalExit ? exit_code : -1);
            });
    connect(&process_,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError process_error) {
                if (process_error == QProcess::FailedToStart) {
                    stopping_ = false;
                    emit failed(process_.errorString());
                }
            });
    connect(&graceful_timer_, &QTimer::timeout, this, [this]() {
        process_.terminate();
        terminate_timer_.start(terminate_timeout_ms_);
    });
    connect(&terminate_timer_, &QTimer::timeout, this, [this]() { process_.kill(); });
}

EngineProcess::~EngineProcess()
{
    if (isRunning()) {
        // Nothing may react to a dying object: waitForFinished delivers the
        // child's death, and that emission must stop here.
        process_.blockSignals(true);
        process_.kill();
        process_.waitForFinished(1000);
    }
}

std::string EngineProcess::generateApiToken()
{
    // system() is the OS CSPRNG (Qt guarantees crypto quality on Linux,
    // Windows and macOS); 32 bytes per launch is far below its bulk limit.
    std::array<quint32, 8> words = {};
    QRandomGenerator::system()->fillRange(words.data(), words.size());

    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string token;
    token.reserve(64);
    for (const auto word : words) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            token += kHexDigits[(word >> shift) & 0xF];
        }
    }
    return token;
}

bool EngineProcess::start(const std::filesystem::path& conf_file, std::string& error)
{
    error.clear();
    if (isRunning()) {
        error = "engine is already running";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(engine_binary_, ec)) {
        error = "engine binary not found: " + engine_binary_.string();
        return false;
    }
    const auto exec_bits = std::filesystem::perms::owner_exec
                         | std::filesystem::perms::group_exec
                         | std::filesystem::perms::others_exec;
    if ((std::filesystem::status(engine_binary_, ec).permissions() & exec_bits)
        == std::filesystem::perms::none) {
        error = "engine binary is not executable: " + engine_binary_.string();
        return false;
    }
    if (!std::filesystem::is_regular_file(conf_file, ec)) {
        error = "conf file not found: " + conf_file.string();
        return false;
    }

    token_ = generateApiToken();
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("DOSBOX_API_TOKEN", QString::fromStdString(token_));
    process_.setProcessEnvironment(env);
    process_.setProgram(QString::fromStdString(engine_binary_.string()));
    process_.setArguments({"-noprimaryconf",
                           "-nolocalconf",
                           "-conf",
                           QString::fromStdString(conf_file.string())});
    stopping_ = false;
    process_.start();
    return true;
}

void EngineProcess::stop()
{
    if (!isRunning() || stopping_) {
        return;
    }
    stopping_ = true;
    if (shutdown_requester_ && shutdown_requester_()) {
        graceful_timer_.start(graceful_timeout_ms_);
    } else {
        process_.terminate();
        terminate_timer_.start(terminate_timeout_ms_);
    }
}

bool EngineProcess::isRunning() const
{
    return process_.state() != QProcess::NotRunning;
}

void EngineProcess::setShutdownRequester(ShutdownRequester requester)
{
    shutdown_requester_ = std::move(requester);
}

void EngineProcess::setStopTimeouts(int graceful_ms, int terminate_ms)
{
    graceful_timeout_ms_ = graceful_ms;
    terminate_timeout_ms_ = terminate_ms;
}

const std::string& EngineProcess::token() const
{
    return token_;
}

} // namespace showroom
