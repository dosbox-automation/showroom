// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_GAME_LAUNCHER_H
#define SHOWROOM_ENGINE_GAME_LAUNCHER_H

#include "engine/api_client.h"
#include "engine/conf_writer.h"
#include "engine/engine_process.h"
#include "model/game_definition.h"

#include <QObject>
#include <QString>

#include <filesystem>
#include <memory>
#include <string>

namespace showroom {

// One launch at a time: conf on disk, child engine, tile-facing signals.
// Virtual so the window can be tested against a fake with no child.
class GameLauncher : public QObject {
    Q_OBJECT

public:
    // ConfWriter pins this port into every run conf, so the client side
    // has to dial the same number.
    static constexpr quint16 kDefaultEnginePort = kShowroomEnginePort;

    GameLauncher(std::filesystem::path engine_binary, std::filesystem::path cache_base,
                 quint16 engine_port = kDefaultEnginePort, QObject* parent = nullptr);
    ~GameLauncher() override;

    virtual bool launch(const GameDefinition& game, std::string& error);
    virtual void stop();
    // Blocks until the child has ended, bounded by the stop escalation
    // plus a margin; returns whether the child is gone. For the
    // showroom's own exit, which must never hang on a stuck emulator.
    virtual bool shutdownAndWait();
    virtual bool isRunning() const;
    virtual QString runningSlug() const;

    void setStopTimeouts(int graceful_ms, int terminate_ms);

signals:
    void gameStarted(const QString& slug);
    void gameEnded(const QString& slug);
    void launchFailed(const QString& slug, const QString& reason);

private:
    static constexpr int kShutdownMarginMs = 2000;

    std::filesystem::path engine_binary_;
    std::filesystem::path cache_base_;
    quint16 engine_port_;
    EngineProcess engine_;
    std::unique_ptr<ApiClient> api_;
    QString running_slug_;
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_GAME_LAUNCHER_H
