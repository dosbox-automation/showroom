// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/game_launcher.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace showroom {
namespace {

GameDefinition doomGame()
{
    const std::string toml = R"(slug = "doom"
title = "DOOM"
rank = 1
license = "shareware"
recipe_status = "done"

[sources.primary]
role = "primary"
install_type = "floppyinstall"
url = "https://example.org/doom.7z"

[dosbox]
machine = "svga_s3"
cpu_cycles = 12000
cpu_cycles_protected = 12000

[launch]
executable = "DOOM.EXE"

[install]
max_runtime_seconds = 120
)";
    std::string error;
    const auto game = GameDefinition::fromTomlString(toml, error);
    EXPECT_TRUE(game) << error;
    return *game;
}

// A port that briefly held an ephemeral listener answers with a refusal
// instead of reaching whatever engine happens to run on the default port.
quint16 deadPort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    return probe.serverPort();
}

class GameLauncherFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::filesystem::create_directories(installDir());
        setenv("FAKE_ENGINE_REPORT", (base() / "report.txt").string().c_str(), 1);
    }

    void TearDown() override
    {
        unsetenv("FAKE_ENGINE_REPORT");
        unsetenv("FAKE_ENGINE_MODE");
        unsetenv("FAKE_ENGINE_SHUTDOWN_FILE");
        unsetenv("FAKE_ENGINE_READY_FILE");
    }

    std::filesystem::path base() const
    {
        return std::filesystem::path(dir_.path().toStdString());
    }

    std::filesystem::path installDir() const { return base() / "installs" / "doom"; }

    static std::filesystem::path fakeEngine()
    {
        return std::filesystem::path(FAKE_ENGINE_PATH);
    }

    QTemporaryDir dir_;
};

TEST_F(GameLauncherFixture, launch_refuses_a_game_without_an_install_directory)
{
    std::filesystem::remove_all(installDir());
    GameLauncher launcher(fakeEngine(), base(), deadPort());
    std::string error;

    EXPECT_FALSE(launcher.launch(doomGame(), error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(launcher.isRunning());
    EXPECT_FALSE(std::filesystem::exists(base() / "run.conf"));
}

TEST_F(GameLauncherFixture, launch_writes_the_conf_and_reports_the_game_running)
{
    GameLauncher launcher(fakeEngine(), base(), deadPort());
    QSignalSpy started(&launcher, &GameLauncher::gameStarted);
    QSignalSpy ended(&launcher, &GameLauncher::gameEnded);
    std::string error;

    ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
    ASSERT_TRUE(started.wait(5000));

    EXPECT_EQ(started.at(0).at(0).toString(), "doom");
    EXPECT_TRUE(std::filesystem::exists(base() / "run.conf"));

    ASSERT_TRUE(ended.wait(5000));
    EXPECT_EQ(ended.at(0).at(0).toString(), "doom");
    EXPECT_FALSE(launcher.isRunning());
    EXPECT_TRUE(launcher.runningSlug().isEmpty());
}

TEST_F(GameLauncherFixture, launch_refuses_while_a_game_runs)
{
    setenv("FAKE_ENGINE_MODE", "run", 1);
    GameLauncher launcher(fakeEngine(), base(), deadPort());
    QSignalSpy started(&launcher, &GameLauncher::gameStarted);
    QSignalSpy ended(&launcher, &GameLauncher::gameEnded);
    std::string error;

    ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
    ASSERT_TRUE(started.wait(5000));
    EXPECT_TRUE(launcher.isRunning());
    EXPECT_EQ(launcher.runningSlug(), "doom");

    EXPECT_FALSE(launcher.launch(doomGame(), error));
    EXPECT_FALSE(error.empty());

    launcher.setStopTimeouts(100, 5000);
    launcher.stop();
    ASSERT_TRUE(ended.wait(5000));
}

TEST_F(GameLauncherFixture, a_spawn_failure_reports_launch_failed)
{
    const auto garbage = base() / "garbage-engine";
    std::ofstream(garbage) << "\x7f not a real executable";
    std::filesystem::permissions(garbage,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    GameLauncher launcher(garbage, base(), deadPort());
    QSignalSpy failed(&launcher, &GameLauncher::launchFailed);
    QSignalSpy started(&launcher, &GameLauncher::gameStarted);
    std::string error;

    ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
    ASSERT_TRUE(failed.wait(5000));

    EXPECT_EQ(failed.at(0).at(0).toString(), "doom");
    EXPECT_FALSE(failed.at(0).at(1).toString().isEmpty());
    EXPECT_EQ(started.count(), 0);
    EXPECT_FALSE(launcher.isRunning());
    EXPECT_TRUE(launcher.runningSlug().isEmpty());
}

TEST_F(GameLauncherFixture, stop_ends_the_running_game)
{
    setenv("FAKE_ENGINE_MODE", "run", 1);
    GameLauncher launcher(fakeEngine(), base(), deadPort());
    QSignalSpy started(&launcher, &GameLauncher::gameStarted);
    QSignalSpy ended(&launcher, &GameLauncher::gameEnded);
    std::string error;

    ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
    ASSERT_TRUE(started.wait(5000));

    launcher.setStopTimeouts(100, 5000);
    launcher.stop();
    ASSERT_TRUE(ended.wait(5000));

    EXPECT_EQ(ended.count(), 1);
    EXPECT_FALSE(launcher.isRunning());
}

TEST_F(GameLauncherFixture, shutdown_and_wait_ends_the_game_before_returning)
{
    setenv("FAKE_ENGINE_MODE", "run", 1);
    GameLauncher launcher(fakeEngine(), base(), deadPort());
    QSignalSpy started(&launcher, &GameLauncher::gameStarted);
    std::string error;

    ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
    ASSERT_TRUE(started.wait(5000));

    launcher.setStopTimeouts(100, 100);
    EXPECT_TRUE(launcher.shutdownAndWait());
    EXPECT_FALSE(launcher.isRunning());
}

TEST_F(GameLauncherFixture, shutdown_and_wait_with_nothing_running_returns_at_once)
{
    GameLauncher launcher(fakeEngine(), base(), deadPort());

    EXPECT_TRUE(launcher.shutdownAndWait());
}

TEST_F(GameLauncherFixture, destroying_the_launcher_with_a_running_child_is_safe)
{
    // The engine member outlives the slug member in reverse destruction
    // order, so a finished signal emitted while the destructor kills the
    // child must not reach the launcher's dead members.
    setenv("FAKE_ENGINE_MODE", "run", 1);
    {
        GameLauncher launcher(fakeEngine(), base(), deadPort());
        QSignalSpy started(&launcher, &GameLauncher::gameStarted);
        std::string error;
        ASSERT_TRUE(launcher.launch(doomGame(), error)) << error;
        ASSERT_TRUE(started.wait(5000));
    }
}

} // namespace
} // namespace showroom
