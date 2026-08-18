// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_INSTALL_RUNNER_H
#define SHOWROOM_ENGINE_INSTALL_RUNNER_H

#include "engine/api_client.h"
#include "engine/conf_writer.h"
#include "engine/engine_process.h"
#include "model/game_definition.h"
#include "net/archive_extractor.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace showroom {

// One install at a time: extract the archive, write the install conf,
// run the recipe through a child engine, poll its status, verify the
// staged result and promote it into installs/<slug>. Disk swaps happen
// inside the recipe (dosbox.drive_swap), so there is no swap
// coordination here. Virtual so the window can be tested against a
// fake with no child.
class InstallRunner : public QObject {
    Q_OBJECT

public:
    InstallRunner(std::filesystem::path engine_binary, std::filesystem::path cache_base,
                  std::filesystem::path games_dir, const ArchiveExtractor& extractor,
                  quint16 engine_port = kShowroomEnginePort, QObject* parent = nullptr);
    ~InstallRunner() override;

    virtual bool startInstall(const GameDefinition& game, std::string& error);
    virtual bool isRunning() const;
    virtual QString installingSlug() const;

    void setStopTimeouts(int graceful_ms, int terminate_ms);

signals:
    void progressChanged(const QString& slug, int percent);
    void succeeded(const QString& slug);
    void failed(const QString& slug, const QString& reason);

private:
    // Starting covers the engine's webserver boot: status probes retry
    // until one answers, then the recipe is loaded and started. Direct
    // is the no-engine path: plain archives whose extraction is the
    // whole install.
    enum class Phase { Idle, Starting, Polling, Stopping, Direct };

    static constexpr int kPollIntervalMs = 1000;
    // Headroom on top of max_runtime_seconds for the engine boot and
    // the script load/start round trips.
    static constexpr int kEngineStartBudgetMs = 15000;
    static constexpr int kMaxConsecutivePollFailures = 5;

    void runDirectInstall();
    void onPollTick();
    void beginScript();
    void handleScriptStatus(const ApiClient::Response& response);
    void stopEngineThen(const QString& pending_failure);
    void onEngineEnded(int exit_code);
    void verifyAndPromote();
    void failInstall(const QString& reason);
    void removeStaging();

    std::filesystem::path engine_binary_;
    std::filesystem::path cache_base_;
    std::filesystem::path games_dir_;
    const ArchiveExtractor& extractor_;
    quint16 engine_port_ = 0;

    EngineProcess engine_;
    std::unique_ptr<ApiClient> api_;
    QTimer poll_timer_;
    QTimer deadline_timer_;

    Phase phase_ = Phase::Idle;
    // Optional because GameDefinition only exists parsed and validated.
    std::optional<GameDefinition> game_;
    QString slug_;
    std::filesystem::path archive_;
    // Where the direct install unpacks: staging itself, or the source's
    // target_subdir below it for flat mirror archives.
    std::filesystem::path direct_extract_dir_;
    std::filesystem::path extracts_dir_;
    std::filesystem::path staging_dir_;
    QByteArray recipe_;
    QString pending_failure_;
    int last_progress_ = -1;
    int poll_failures_ = 0;
    bool request_in_flight_ = false;
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_INSTALL_RUNNER_H
