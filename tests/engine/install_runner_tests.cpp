// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/install_runner.h"

#include "engine/conf_writer.h"
#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace showroom {
namespace {

namespace fs = std::filesystem;

GameDefinition installableGame(int max_runtime_seconds = 30,
                               const std::string& install_type = "floppyinstall",
                               const std::string& archive = "doom.7z")
{
    const std::string toml = std::format(R"(slug = "doom"
title = "DOOM"
rank = 1
license = "shareware"
recipe_status = "done"

[sources.primary]
role = "primary"
install_type = "{}"
url = "https://example.org/{}"

[dosbox]
machine = "svga_s3"
cpu_cycles = 12000
cpu_cycles_protected = 12000

[launch]
executable = "DOOM.EXE"
working_dir = "GOLD/DOOM"

[install]
max_runtime_seconds = {}

[install.expected_files]
"GOLD/DOOM/DOOM.EXE" = {{ size = 5 }}
)",
                                         install_type,
                                         archive,
                                         max_runtime_seconds);
    std::string error;
    const auto game = GameDefinition::fromTomlString(toml, error);
    EXPECT_TRUE(game) << error;
    return *game;
}

// Routes the engine API endpoints the runner drives: fixed answers for
// status/load/start, a scripted sequence for script/status, and the
// fake engine's shutdown file as the side effect of the shutdown route.
class ScriptedEngineServer : public QObject {
public:
    ScriptedEngineServer()
    {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            auto* socket = server_.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
                *buffer += socket->readAll();
                if (!requestComplete(*buffer)) {
                    return;
                }
                socket->write(responseFor(*buffer));
                socket->flush();
                socket->disconnectFromHost();
            });
        });
        server_.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return server_.serverPort(); }

    void queueScriptStatus(const QByteArray& body) { status_bodies_.push_back(body); }
    void setShutdownFile(const fs::path& path) { shutdown_file_ = path; }

    int shutdownRequests() const { return shutdown_requests_; }

private:
    static bool requestComplete(const QByteArray& data)
    {
        const auto header_end = data.indexOf("\r\n\r\n");
        if (header_end < 0) {
            return false;
        }
        int expected = 0;
        for (const auto& line : data.left(header_end).split('\n')) {
            if (line.toLower().startsWith("content-length:")) {
                expected = line.mid(15).trimmed().toInt();
            }
        }
        return data.size() >= header_end + 4 + expected;
    }

    static QByteArray wrap(const QByteArray& body)
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Content-Length: "
             + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    }

    QByteArray responseFor(const QByteArray& request)
    {
        const QByteArray line = request.left(request.indexOf("\r\n"));
        if (line.contains("/api/v1/script/status")) {
            const QByteArray body = status_bodies_.empty()
                                          ? QByteArray(R"({"state":"running"})")
                                          : status_bodies_.front();
            if (status_bodies_.size() > 1) {
                status_bodies_.erase(status_bodies_.begin());
            }
            return wrap(body);
        }
        if (line.contains("/api/v1/dosbox/shutdown")) {
            ++shutdown_requests_;
            if (!shutdown_file_.empty()) {
                std::ofstream(shutdown_file_) << "shutdown";
            }
            return wrap(R"({"status":"shutdown_requested"})");
        }
        if (line.contains("/api/v1/script/load")) {
            return wrap(R"({"status":"loaded"})");
        }
        if (line.contains("/api/v1/script/start")) {
            return wrap(R"({"status":"started"})");
        }
        return wrap(R"({"running":true})");
    }

    QTcpServer server_;
    std::vector<QByteArray> status_bodies_;
    fs::path shutdown_file_;
    int shutdown_requests_ = 0;
};

class FakeArchiveExtractor : public ArchiveExtractor {
public:
    ExtractResult extract(const fs::path& archive,
                          const fs::path& destination) const override
    {
        ++calls;
        last_archive = archive;
        last_destination = destination;
        if (fail) {
            return {false, "planted failure"};
        }
        fs::create_directories(destination);
        if (plant_install) {
            fs::create_directories(destination / "GOLD" / "DOOM");
            std::ofstream(destination / "GOLD" / "DOOM" / "DOOM.EXE") << "12345";
        } else {
            std::ofstream(destination / "disk1.ima") << "image";
        }
        return {true, ""};
    }

    mutable int calls = 0;
    mutable fs::path last_archive;
    mutable fs::path last_destination;
    bool fail = false;
    // Plain-archive installs unpack the finished game, not disk images.
    bool plant_install = false;
};

class InstallRunnerFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        // House rule: scratch lives in the project .workspace, not /tmp.
        base_ = fs::path(SHOWROOM_TEST_WORKSPACE)
              / ("install-runner-"
                 + std::to_string(testing::UnitTest::GetInstance()->random_seed()) + "-"
                 + info->name());
        fs::remove_all(base_);
        cache_ = base_ / "cache";
        games_ = base_ / "games";
        fs::create_directories(cache_ / "downloads" / "doom");
        fs::create_directories(games_ / "doom");
        std::ofstream(cache_ / "downloads" / "doom" / "doom.7z") << "archive";
        std::ofstream(games_ / "doom" / "recipe.lua") << "-- recipe\n";

        server_.setShutdownFile(base_ / "shutdown");
        setenv("FAKE_ENGINE_MODE", "run", 1);
        setenv("FAKE_ENGINE_SHUTDOWN_FILE", (base_ / "shutdown").string().c_str(), 1);

        runner_ = std::make_unique<InstallRunner>(fs::path(FAKE_ENGINE_PATH),
                                                  cache_,
                                                  games_,
                                                  extractor_,
                                                  server_.port());
        runner_->setStopTimeouts(300, 300);
    }

    void TearDown() override
    {
        runner_.reset();
        unsetenv("FAKE_ENGINE_MODE");
        unsetenv("FAKE_ENGINE_SHUTDOWN_FILE");
        fs::remove_all(base_);
    }

    fs::path stagingDir() const
    {
        return ConfWriter::installStagingDir(cache_ / "extracts" / "doom");
    }

    void plantStagedResult()
    {
        const auto dir = stagingDir() / "GOLD" / "DOOM";
        fs::create_directories(dir);
        std::ofstream(dir / "DOOM.EXE") << "12345";
    }

    // Signals arrive on the event loop; pump with a ceiling, never wait
    // unbounded on a stuck emulator.
    static bool pumpUntil(QSignalSpy& spy, int timeout_ms)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        while (spy.isEmpty() && elapsed.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return !spy.isEmpty();
    }

    fs::path base_;
    fs::path cache_;
    fs::path games_;
    ScriptedEngineServer server_;
    FakeArchiveExtractor extractor_;
    std::unique_ptr<InstallRunner> runner_;
};

TEST_F(InstallRunnerFixture, a_full_install_extracts_verifies_and_promotes)
{
    server_.queueScriptStatus(R"({"state":"running","output":{"progress":10}})");
    server_.queueScriptStatus(R"({"state":"running","output":{"progress":"50"}})");
    server_.queueScriptStatus(
            R"({"state":"completed","output":{"progress":100,"install_complete":"yes"}})");

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    QSignalSpy progress(runner_.get(), &InstallRunner::progressChanged);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;
    EXPECT_TRUE(runner_->isRunning());
    EXPECT_EQ(runner_->installingSlug(), "doom");
    plantStagedResult();

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_FALSE(runner_->isRunning());
    EXPECT_EQ(extractor_.calls, 1);
    EXPECT_TRUE(fs::is_regular_file(cache_ / "installs" / "doom" / "GOLD" / "DOOM"
                                    / "DOOM.EXE"));
    EXPECT_FALSE(fs::exists(stagingDir()));

    std::vector<int> seen;
    for (const auto& arguments : progress) {
        seen.push_back(arguments.at(1).toInt());
    }
    EXPECT_EQ(seen, (std::vector<int>{10, 50, 100}));
}

TEST_F(InstallRunnerFixture, a_recipe_error_rolls_back_and_spares_everything_else)
{
    fs::create_directories(cache_ / "installs" / "doom");
    std::ofstream(cache_ / "installs" / "doom" / "OLD.TXT") << "previous install";
    server_.queueScriptStatus(R"({"state":"error","error":"installer never showed"})");

    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("installer never showed"));
    EXPECT_FALSE(fs::exists(stagingDir()));
    EXPECT_TRUE(fs::exists(cache_ / "downloads" / "doom" / "doom.7z"));
    EXPECT_TRUE(fs::exists(cache_ / "extracts" / "doom" / "disk1.ima"));
    EXPECT_TRUE(fs::exists(cache_ / "installs" / "doom" / "OLD.TXT"));
}

TEST_F(InstallRunnerFixture, a_script_overrunning_its_ceiling_is_stopped_and_rolled_back)
{
    // No completed status ever arrives; the 1 s ceiling must fire.
    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(1), error)) << error;

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("timed out"));
    EXPECT_FALSE(runner_->isRunning());
    EXPECT_FALSE(fs::exists(stagingDir()));
    EXPECT_GE(server_.shutdownRequests(), 1);
}

TEST_F(InstallRunnerFixture, an_engine_that_dies_mid_install_rolls_back)
{
    setenv("FAKE_ENGINE_MODE", "exit", 1);

    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("unexpectedly"));
    EXPECT_FALSE(fs::exists(stagingDir()));
}

TEST_F(InstallRunnerFixture, a_completed_script_without_the_completion_signal_fails)
{
    server_.queueScriptStatus(R"({"state":"completed","output":{}})");

    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;
    plantStagedResult();

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("install_complete"));
    EXPECT_FALSE(fs::exists(cache_ / "installs" / "doom"));
}

TEST_F(InstallRunnerFixture, missing_expected_files_fail_verification_and_roll_back)
{
    server_.queueScriptStatus(
            R"({"state":"completed","output":{"install_complete":"yes"}})");

    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("verification failed"));
    EXPECT_FALSE(fs::exists(stagingDir()));
    EXPECT_FALSE(fs::exists(cache_ / "installs" / "doom"));
}

TEST_F(InstallRunnerFixture, a_populated_extracts_dir_skips_extraction)
{
    fs::create_directories(cache_ / "extracts" / "doom");
    std::ofstream(cache_ / "extracts" / "doom" / "disk1.ima") << "image";
    server_.queueScriptStatus(
            R"({"state":"completed","output":{"install_complete":"yes"}})");

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;
    plantStagedResult();

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_EQ(extractor_.calls, 0);
}

TEST_F(InstallRunnerFixture, an_exe_install_copies_the_installer_into_extracts_untouched)
{
    // The self-extractor runs inside the machine; the host-side
    // "extraction" is a copy, timestamp kept, libarchive never called.
    std::ofstream(cache_ / "downloads" / "doom" / "INSTALL.EXE") << "MZ";
    const auto planted = fs::file_time_type::clock::now() - std::chrono::hours(24);
    fs::last_write_time(cache_ / "downloads" / "doom" / "INSTALL.EXE", planted);
    server_.queueScriptStatus(
            R"({"state":"completed","output":{"install_complete":"yes"}})");

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(30, "exeinstall", "INSTALL.EXE"),
                                      error))
            << error;
    plantStagedResult();

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_EQ(extractor_.calls, 0);
    const auto copied = cache_ / "extracts" / "doom" / "INSTALL.EXE";
    ASSERT_TRUE(fs::is_regular_file(copied));
    EXPECT_EQ(fs::last_write_time(copied), planted);
}

TEST_F(InstallRunnerFixture, a_plain_archive_installs_without_engine_or_recipe)
{
    fs::remove(games_ / "doom" / "recipe.lua");
    extractor_.plant_install = true;

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(30, "unzip"), error)) << error;
    EXPECT_TRUE(runner_->isRunning());

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_FALSE(runner_->isRunning());
    EXPECT_EQ(extractor_.calls, 1);
    EXPECT_EQ(extractor_.last_destination, stagingDir());
    EXPECT_TRUE(fs::is_regular_file(cache_ / "installs" / "doom" / "GOLD" / "DOOM"
                                    / "DOOM.EXE"));
    EXPECT_FALSE(fs::exists(stagingDir()));
    EXPECT_EQ(server_.shutdownRequests(), 0);
}

TEST_F(InstallRunnerFixture, the_overlay_lands_on_top_of_a_plain_archive_install)
{
    extractor_.plant_install = true;
    const auto overlay = games_ / "doom" / "overlay" / "GOLD" / "DOOM";
    fs::create_directories(overlay);
    std::ofstream(overlay / "SOUND.CFG") << "sb16";

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(30, "unzip"), error)) << error;

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_TRUE(fs::is_regular_file(cache_ / "installs" / "doom" / "GOLD" / "DOOM"
                                    / "SOUND.CFG"));
}

TEST_F(InstallRunnerFixture, the_overlay_lands_on_engine_driven_installs_too)
{
    const auto overlay = games_ / "doom" / "overlay" / "GOLD" / "DOOM";
    fs::create_directories(overlay);
    std::ofstream(overlay / "SOUND.CFG") << "sb16";
    server_.queueScriptStatus(
            R"({"state":"completed","output":{"install_complete":"yes"}})");

    QSignalSpy succeeded(runner_.get(), &InstallRunner::succeeded);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(), error)) << error;
    plantStagedResult();

    ASSERT_TRUE(pumpUntil(succeeded, 20000));
    EXPECT_TRUE(fs::is_regular_file(cache_ / "installs" / "doom" / "GOLD" / "DOOM"
                                    / "SOUND.CFG"));
}

TEST_F(InstallRunnerFixture, a_failed_plain_archive_extraction_rolls_back)
{
    extractor_.fail = true;
    QSignalSpy failed(runner_.get(), &InstallRunner::failed);
    std::string error;
    ASSERT_TRUE(runner_->startInstall(installableGame(30, "unzip"), error)) << error;

    ASSERT_TRUE(pumpUntil(failed, 20000));
    EXPECT_TRUE(failed.front().at(1).toString().contains("extraction failed"));
    EXPECT_FALSE(fs::exists(stagingDir()));
    EXPECT_TRUE(fs::exists(cache_ / "downloads" / "doom" / "doom.7z"));
}

TEST_F(InstallRunnerFixture, refuses_a_game_without_a_recipe_file)
{
    fs::remove(games_ / "doom" / "recipe.lua");
    std::string error;
    EXPECT_FALSE(runner_->startInstall(installableGame(), error));
    EXPECT_NE(error.find("recipe"), std::string::npos) << error;
}

TEST_F(InstallRunnerFixture, refuses_a_missing_archive)
{
    fs::remove(cache_ / "downloads" / "doom" / "doom.7z");
    std::string error;
    EXPECT_FALSE(runner_->startInstall(installableGame(), error));
    EXPECT_NE(error.find("archive"), std::string::npos) << error;
}

TEST_F(InstallRunnerFixture, a_failed_extraction_cleans_the_extracts_dir)
{
    extractor_.fail = true;
    std::string error;
    EXPECT_FALSE(runner_->startInstall(installableGame(), error));
    EXPECT_NE(error.find("extraction failed"), std::string::npos) << error;
    // A partial extraction left behind would be mistaken for a complete
    // one on the next attempt.
    EXPECT_FALSE(fs::exists(cache_ / "extracts" / "doom"));
}

} // namespace
} // namespace showroom
