// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_ENGINE_PROCESS_H
#define SHOWROOM_ENGINE_ENGINE_PROCESS_H

#include <QObject>
#include <QProcess>
#include <QTimer>

#include <filesystem>
#include <functional>
#include <string>

namespace showroom {

// Owns one dosbox-automation child. Stop order is shutdown endpoint,
// then terminate, then kill - always addressed to this child's pid,
// never to a process name.
class EngineProcess : public QObject {
    Q_OBJECT

public:
    using ShutdownRequester = std::function<bool()>;

    explicit EngineProcess(std::filesystem::path engine_binary,
                           QObject* parent = nullptr);
    ~EngineProcess() override;

    static std::string generateApiToken();

    bool start(const std::filesystem::path& conf_file, std::string& error);
    void stop();
    bool isRunning() const;

    void setShutdownRequester(ShutdownRequester requester);
    void setStopTimeouts(int graceful_ms, int terminate_ms);

    // Worst case from stop() to the kill, for callers bounding a wait.
    int stopEscalationBudgetMs() const
    {
        return graceful_timeout_ms_ + terminate_timeout_ms_;
    }

    const std::string& token() const;

signals:
    void started();
    // Emitted for every end of a running child; exit_code is -1 when the
    // child did not leave through a normal exit.
    void finished(int exit_code);
    // Emitted only when the child never ran at all.
    void failed(const QString& reason);

private:
    std::filesystem::path engine_binary_;
    std::string token_;
    QProcess process_;
    QTimer graceful_timer_;
    QTimer terminate_timer_;
    ShutdownRequester shutdown_requester_;
    int graceful_timeout_ms_ = 20000;
    int terminate_timeout_ms_ = 5000;
    bool stopping_ = false;
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_ENGINE_PROCESS_H
