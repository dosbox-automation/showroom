// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/main_window.h"

#include "app/settings.h"
#include "engine/game_launcher.h"
#include "engine/install_runner.h"
#include "model/game_catalog.h"
#include "model/step_sizer.h"
#include "net/connectivity.h"
#include "net/downloader.h"
#include "ui/game_tile.h"
#include "ui/tile_grid.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <vector>

namespace showroom {
namespace {

std::filesystem::path assetsDir()
{
    return std::filesystem::path(SHOWROOM_SOURCE_ASSETS_DIR);
}

const GameCatalog& catalog()
{
    static const GameCatalog kLoaded = GameCatalog::loadFromDirectory(assetsDir()
                                                                      / "games");
    return kLoaded;
}

// The settings file must never be the real one: a test that rewrites the
// developer's window size is a test that changes the machine it runs on.
class WindowFixture : public ::testing::Test {
protected:
    Settings settings()
    {
        return Settings(std::filesystem::path(dir_.path().toStdString())
                        / "showroom.ini");
    }

    // A stated screen rather than whatever the platform plugin reports.
    // The offscreen plugin runs at 800x600, which holds no step at all,
    // so a window test that trusted it would be testing the plugin.
    static StepSizer sizer(int width_px = 1920, int height_px = 1080)
    {
        return StepSizer(GridChrome{}, width_px, height_px);
    }

    QTemporaryDir dir_;
};

TEST_F(WindowFixture, the_window_opens_at_a_step_the_screen_can_hold)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    const std::vector<int>& steps = window.sizer().tileWidths();
    ASSERT_FALSE(steps.empty());
    EXPECT_NE(std::find(steps.begin(), steps.end(), window.tileWidth()), steps.end());
}

TEST_F(WindowFixture, the_window_is_exactly_the_size_its_step_implies)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    const WindowSize expected = window.sizer().windowSizeFor(window.tileWidth());
    EXPECT_EQ(window.size().width(), expected.width_px);
    EXPECT_EQ(window.size().height(), expected.height_px);
}

TEST_F(WindowFixture, stepping_up_and_down_returns_to_where_it_started)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());
    const int started_at = window.tileWidth();

    window.stepUp();
    EXPECT_GT(window.tileWidth(), started_at);
    window.stepDown();
    EXPECT_EQ(window.tileWidth(), started_at);
}

TEST_F(WindowFixture, stepping_past_the_end_stays_at_the_end)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    for (int i = 0; i < 20; ++i) {
        window.stepUp();
    }
    EXPECT_EQ(window.tileWidth(), window.sizer().tileWidths().back());

    for (int i = 0; i < 20; ++i) {
        window.stepDown();
    }
    EXPECT_EQ(window.tileWidth(), window.sizer().tileWidths().front());
}

TEST_F(WindowFixture, the_chosen_step_survives_a_restart)
{
    const int chosen = [this] {
        MainWindow window(catalog(), assetsDir(), settings(), sizer());
        window.stepUp();
        return window.tileWidth();
    }();

    MainWindow reopened(catalog(), assetsDir(), settings(), sizer());
    EXPECT_EQ(reopened.tileWidth(), chosen);
}

TEST_F(WindowFixture, a_stored_width_this_screen_cannot_hold_is_snapped_not_obeyed)
{
    Settings stored = settings();
    stored.setTileWidth(3000);

    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    const std::vector<int>& steps = window.sizer().tileWidths();
    EXPECT_NE(std::find(steps.begin(), steps.end(), window.tileWidth()), steps.end());
    EXPECT_EQ(window.tileWidth(), steps.back());
}

TEST_F(WindowFixture, a_corrupt_settings_file_falls_back_to_the_default_step)
{
    const std::filesystem::path file = std::filesystem::path(dir_.path().toStdString())
                                     / "showroom.ini";
    {
        std::ofstream out(file);
        out << "[window]\ntile_width_px=not-a-number\n";
    }

    MainWindow window(catalog(), assetsDir(), Settings(file), sizer());
    EXPECT_EQ(window.tileWidth(), window.sizer().defaultTileWidth());
}

TEST_F(WindowFixture, the_grid_holds_one_tile_per_bundled_game)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    const auto* grid = window.findChild<TileGrid*>();
    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(grid->tileCount(), static_cast<int>(catalog().size()));
    EXPECT_NE(grid->tileFor(QStringLiteral("doom")), nullptr);
    EXPECT_EQ(grid->tileFor(QStringLiteral("no-such-game")), nullptr);
}

TEST_F(WindowFixture, every_tile_follows_the_step)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());
    window.stepUp();

    const auto* grid = window.findChild<TileGrid*>();
    ASSERT_NE(grid, nullptr);
    const GameTile* tile = grid->tileFor(QStringLiteral("doom"));
    ASSERT_NE(tile, nullptr);
    EXPECT_EQ(tile->width(), window.tileWidth());
}

TEST_F(WindowFixture, the_window_accepts_a_close_request)
{
    // The application side of the title bar X. Worth a test because the
    // desktop half depends on a keybinding that can be missing, and then
    // a manual check proves nothing either way.
    MainWindow window(catalog(), assetsDir(), settings(), sizer());
    window.show();

    EXPECT_TRUE(window.close());
    EXPECT_FALSE(window.isVisible());
}

TEST_F(WindowFixture, a_screen_too_small_for_any_step_still_opens_a_window)
{
    // No step fits on a 640x480 display. Opening at the minimum and
    // overflowing beats refusing to start, and nothing in the window may
    // reach into the empty step list to find that out.
    MainWindow window(catalog(), assetsDir(), settings(), sizer(640, 480));

    EXPECT_TRUE(window.sizer().tileWidths().empty());
    EXPECT_EQ(window.tileWidth(), kMinTileWidthPx);

    window.stepUp();
    window.stepDown();
    EXPECT_EQ(window.tileWidth(), kMinTileWidthPx);
}

// Records what the window asked for and plays the launcher's signals
// back, so the wiring is tested without any child process.
class FakeLauncher : public GameLauncher {
public:
    FakeLauncher() : GameLauncher("/nonexistent/engine", "/nonexistent/cache") {}

    bool launch(const GameDefinition& game, std::string& error) override
    {
        if (refuse_launch) {
            error = "refused by the fake";
            return false;
        }
        launched_slugs.push_back(game.slug());
        return true;
    }

    void stop() override { ++stop_calls; }

    bool shutdownAndWait() override
    {
        ++shutdown_waits;
        fake_running_slug.clear();
        return true;
    }

    bool isRunning() const override { return !fake_running_slug.isEmpty(); }
    QString runningSlug() const override { return fake_running_slug; }

    void simulateStarted(const QString& slug) { emit gameStarted(slug); }
    void simulateEnded(const QString& slug) { emit gameEnded(slug); }
    void simulateFailed(const QString& slug, const QString& reason)
    {
        emit launchFailed(slug, reason);
    }

    std::vector<std::string> launched_slugs;
    int stop_calls = 0;
    int shutdown_waits = 0;
    bool refuse_launch = false;
    QString fake_running_slug;
};

// Records what the window asked for; the tests play the signals back.
class FakeDownloader : public Downloader {
public:
    bool start(const QUrl& url, const std::filesystem::path& destination,
               std::string& error,
               std::optional<std::uint64_t> expected_size_bytes) override
    {
        if (busy) {
            error = "a transfer is already running";
            return false;
        }
        ++starts;
        busy = true;
        last_url = url;
        last_destination = destination;
        last_expected_size = expected_size_bytes;
        return true;
    }

    void cancel() override { ++cancel_calls; }
    bool isRunning() const override { return busy; }

    void simulateProgress(qint64 received, qint64 total)
    {
        emit progress(received, total);
    }
    void simulateFinished(const QString& path)
    {
        busy = false;
        emit finished(path);
    }
    void simulateFailed(const QString& reason)
    {
        busy = false;
        emit failed(reason);
    }
    void simulateCancelled()
    {
        busy = false;
        emit cancelled();
    }

    int starts = 0;
    int cancel_calls = 0;
    bool busy = false;
    QUrl last_url;
    std::filesystem::path last_destination;
    std::optional<std::uint64_t> last_expected_size;
};

class LaunchFixture : public WindowFixture {
protected:
    void SetUp() override
    {
        const char* saved = std::getenv("SHOWROOM_CACHE_DIR");
        saved_cache_ = saved ? std::optional<std::string>(saved) : std::nullopt;
        cache_ = std::filesystem::path(dir_.path().toStdString()) / "cache";
        qputenv("SHOWROOM_CACHE_DIR", QByteArray(cache_.string().c_str()));
        std::filesystem::create_directories(cache_ / "installs" / "doom");
    }

    void TearDown() override
    {
        if (saved_cache_.has_value()) {
            qputenv("SHOWROOM_CACHE_DIR", QByteArray(saved_cache_->c_str()));
        } else {
            qunsetenv("SHOWROOM_CACHE_DIR");
        }
    }

    static void writeGame(const std::filesystem::path& games_dir, const std::string& slug,
                          const std::string& title, int rank,
                          const std::string& extra_toml = {},
                          const std::string& executable = "GAME.EXE")
    {
        std::filesystem::create_directories(games_dir / slug);
        std::ofstream out(games_dir / slug / (slug + ".toml"));
        out << "slug = \"" << slug << "\"\n"
            << "title = \"" << title << "\"\n"
            << "rank = " << rank << "\n"
            << "license = \"shareware\"\n"
            << "[sources.primary]\n"
            << "url = \"https://example.invalid/" << slug << ".zip\"\n"
            << "[dosbox]\n"
            << "machine = \"svga_s3\"\n"
            << "cpu_cycles = 3000\n"
            << "cpu_cycles_protected = 3000\n"
            << "[launch]\n"
            << "executable = \"" << executable << "\"\n"
            << "[install]\n"
            << "max_runtime_seconds = 60\n"
            << extra_toml;
    }

    std::filesystem::path cache_;
    std::optional<std::string> saved_cache_;
};

TEST_F(LaunchFixture, a_tile_with_an_install_directory_starts_ready)
{
    // A leftover install directory for a game without a launch executable
    // must not put Play on its tile. Both subjects are built here: every
    // bundled game has a recipe now.
    const auto games_dir = std::filesystem::path(dir_.path().toStdString()) / "games";
    writeGame(games_dir, "alpha", "Alpha", 1);
    writeGame(games_dir, "norecipe", "No Recipe", 2, {}, "");
    const auto games = GameCatalog::loadFromDirectory(games_dir);
    ASSERT_EQ(games.size(), 2u);
    std::filesystem::create_directories(cache_ / "installs" / "alpha");
    std::filesystem::create_directories(cache_ / "installs" / "norecipe");

    FakeLauncher launcher;
    MainWindow window(games, assetsDir(), settings(), sizer(), &launcher);

    ASSERT_NE(window.grid()->tileFor("alpha"), nullptr);
    EXPECT_EQ(window.grid()->tileFor("alpha")->state(), TileState::Ready);
    EXPECT_EQ(window.grid()->tileFor("norecipe")->state(), TileState::NoRecipe);
}

TEST_F(LaunchFixture, without_a_launcher_no_tile_reads_the_cache)
{
    MainWindow window(catalog(), assetsDir(), settings(), sizer());

    EXPECT_EQ(window.grid()->tileFor("doom")->state(), TileState::NotDownloaded);
}

TEST_F(LaunchFixture, the_tile_follows_the_launcher_through_a_run)
{
    FakeLauncher launcher;
    MainWindow window(catalog(), assetsDir(), settings(), sizer(), &launcher);
    GameTile* tile = window.grid()->tileFor("doom");

    launcher.simulateStarted("doom");
    EXPECT_EQ(tile->state(), TileState::Running);

    launcher.simulateEnded("doom");
    EXPECT_EQ(tile->state(), TileState::Ready);
}

// Answers the confirmation dialogs from a script; a modal QMessageBox
// under the offscreen platform would hang the test.
class ScriptedWindow : public MainWindow {
public:
    using MainWindow::MainWindow;

    bool accept_switch = false;
    int questions_asked = 0;
    QString named_running_title;

    bool accept_reinstall = false;
    int reinstall_offers = 0;
    QString named_reinstall_title;

    bool accept_port_notice = true;
    bool tick_dont_show_again = false;
    int port_notices = 0;

protected:
    bool confirmPortNotice(bool& dont_show_again) override
    {
        ++port_notices;
        dont_show_again = tick_dont_show_again;
        return accept_port_notice;
    }

    bool confirmGameSwitch(const QString& running_title,
                           const QString& pending_title) override
    {
        ++questions_asked;
        named_running_title = running_title;
        (void)pending_title;
        return accept_switch;
    }

    bool offerReinstall(const QString& title, const QString& detail) override
    {
        ++reinstall_offers;
        named_reinstall_title = title;
        (void)detail;
        return accept_reinstall;
    }
};

// A switch needs two games whose install state this test controls, so
// this fixture builds its own catalogue through the real loader.
class SwitchFixture : public LaunchFixture {
protected:
    void SetUp() override
    {
        LaunchFixture::SetUp();
        const auto games_dir = std::filesystem::path(dir_.path().toStdString()) / "games";
        writeGame(games_dir, "alpha", "Alpha", 1);
        writeGame(games_dir,
                  "beta",
                  "Beta",
                  2,
                  "[install.expected_files]\n\"DATA.DAT\" = { size = 4 }\n");
        std::filesystem::create_directories(cache_ / "installs" / "alpha");
        std::filesystem::create_directories(cache_ / "installs" / "beta");
        writeBetaData(4);
        two_games_ = GameCatalog::loadFromDirectory(games_dir);
        ASSERT_EQ(two_games_.size(), 2u);
    }

    void writeBetaData(std::size_t byte_count)
    {
        std::ofstream out(cache_ / "installs" / "beta" / "DATA.DAT",
                          std::ios::binary | std::ios::trunc);
        out << std::string(byte_count, 'x');
    }

    GameCatalog two_games_;
};

TEST_F(SwitchFixture, the_action_on_a_running_tile_is_stop)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(launcher.stop_calls, 1);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(SwitchFixture, a_launch_failure_leaves_the_tile_ready)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);
    launcher.simulateFailed(QStringLiteral("alpha"),
                            QStringLiteral("the engine went missing"));

    EXPECT_EQ(tile->state(), TileState::Ready);
}

TEST_F(SwitchFixture, a_refused_launch_keeps_the_tile_ready)
{
    FakeLauncher launcher;
    launcher.refuse_launch = true;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);

    EXPECT_EQ(tile->state(), TileState::Ready);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(SwitchFixture, the_first_play_asks_about_the_port_before_launching)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(window.port_notices, 1);
    ASSERT_EQ(launcher.launched_slugs.size(), 1u);
    EXPECT_EQ(launcher.launched_slugs.front(), "alpha");
}

TEST_F(SwitchFixture, declining_the_port_notice_stops_the_launch)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    window.accept_port_notice = false;

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(window.port_notices, 1);
    EXPECT_TRUE(launcher.launched_slugs.empty());
    EXPECT_EQ(window.grid()->tileFor("alpha")->state(), TileState::Ready);
}

TEST_F(SwitchFixture, dont_show_again_holds_for_the_session)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    window.tick_dont_show_again = true;

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);
    launcher.simulateEnded(QStringLiteral("alpha"));
    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(window.port_notices, 1);
    EXPECT_EQ(launcher.launched_slugs.size(), 2u);
}

TEST_F(SwitchFixture, dont_show_again_survives_a_restart)
{
    FakeLauncher launcher;
    {
        ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
        window.tick_dont_show_again = true;
        QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);
        launcher.simulateEnded(QStringLiteral("alpha"));
    }

    ScriptedWindow reopened(two_games_, assetsDir(), settings(), sizer(), &launcher);
    QTest::mouseClick(reopened.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(reopened.port_notices, 0);
}

TEST_F(SwitchFixture, a_declined_notice_never_suppresses_the_next_one)
{
    // The checkbox only counts on yes: declining with it ticked would
    // otherwise silently disable launching for good.
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    window.accept_port_notice = false;
    window.tick_dont_show_again = true;

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);
    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(window.port_notices, 2);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(SwitchFixture, closing_while_a_game_runs_shuts_it_down_first)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));

    EXPECT_TRUE(window.close());
    EXPECT_EQ(launcher.shutdown_waits, 1);
}

TEST_F(SwitchFixture, closing_with_nothing_running_waits_for_no_one)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);

    EXPECT_TRUE(window.close());
    EXPECT_EQ(launcher.shutdown_waits, 0);
}

TEST_F(SwitchFixture, play_with_nothing_running_asks_no_question)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);

    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    EXPECT_EQ(window.questions_asked, 0);
    ASSERT_EQ(launcher.launched_slugs.size(), 1u);
    EXPECT_EQ(launcher.launched_slugs.front(), "beta");
}

TEST_F(SwitchFixture, declining_a_switch_leaves_the_running_game_alone)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));

    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    EXPECT_EQ(window.questions_asked, 1);
    EXPECT_EQ(window.named_running_title, QStringLiteral("Alpha"));
    EXPECT_EQ(launcher.stop_calls, 0);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(SwitchFixture,
       accepting_a_switch_starts_the_second_game_only_after_the_first_exits)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));
    window.accept_switch = true;

    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    EXPECT_EQ(launcher.stop_calls, 1);
    EXPECT_TRUE(launcher.launched_slugs.empty());

    launcher.fake_running_slug.clear();
    launcher.simulateEnded(QStringLiteral("alpha"));

    ASSERT_EQ(launcher.launched_slugs.size(), 1u);
    EXPECT_EQ(launcher.launched_slugs.front(), "beta");
}

TEST_F(SwitchFixture, a_pending_switch_fires_only_once)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));
    window.accept_switch = true;
    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);
    launcher.fake_running_slug.clear();
    launcher.simulateEnded(QStringLiteral("alpha"));

    launcher.simulateEnded(QStringLiteral("beta"));

    EXPECT_EQ(launcher.launched_slugs.size(), 1u);
}

TEST_F(SwitchFixture, a_game_damaged_during_the_switch_gets_the_offer_not_a_launch)
{
    // The files can change between accepting the switch and the running
    // game's exit, so the pending launch re-checks them.
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    launcher.fake_running_slug = QStringLiteral("alpha");
    launcher.simulateStarted(QStringLiteral("alpha"));
    window.accept_switch = true;
    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    writeBetaData(2);
    launcher.fake_running_slug.clear();
    launcher.simulateEnded(QStringLiteral("alpha"));

    EXPECT_EQ(window.reinstall_offers, 1);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

// One game whose definition names an expected file, so a test controls
// whether the install is intact by writing that file.
class IntegrityFixture : public LaunchFixture {
protected:
    void SetUp() override
    {
        LaunchFixture::SetUp();
        const auto games_dir = std::filesystem::path(dir_.path().toStdString()) / "games";
        writeGame(games_dir,
                  "gamma",
                  "Gamma",
                  1,
                  "[install.expected_files]\n\"DATA.DAT\" = { size = 4 }\n");
        std::filesystem::create_directories(cache_ / "installs" / "gamma");
        one_game_ = GameCatalog::loadFromDirectory(games_dir);
        ASSERT_EQ(one_game_.size(), 1u);
    }

    void writeGammaData(std::size_t byte_count)
    {
        std::ofstream out(cache_ / "installs" / "gamma" / "DATA.DAT",
                          std::ios::binary | std::ios::trunc);
        out << std::string(byte_count, 'x');
    }

    GameCatalog one_game_;
};

// Two launchable games with no install directory, so their tiles offer
// the download action.
class DownloadFixture : public LaunchFixture {
protected:
    void SetUp() override
    {
        LaunchFixture::SetUp();
        const auto games_dir = std::filesystem::path(dir_.path().toStdString()) / "games";
        writeSizedGame(games_dir, "delta", "Delta", 1);
        writeSizedGame(games_dir, "epsilon", "Epsilon", 2);
        two_games_ = GameCatalog::loadFromDirectory(games_dir);
        ASSERT_EQ(two_games_.size(), 2u);
    }

    static void writeSizedGame(const std::filesystem::path& games_dir,
                               const std::string& slug, const std::string& title,
                               int rank)
    {
        std::filesystem::create_directories(games_dir / slug);
        std::ofstream out(games_dir / slug / (slug + ".toml"));
        out << "slug = \"" << slug << "\"\n"
            << "title = \"" << title << "\"\n"
            << "rank = " << rank << "\n"
            << "license = \"shareware\"\n"
            << "[sources.primary]\n"
            << "url = \"https://example.invalid/" << slug << ".zip\"\n"
            << "size = 1000\n"
            << "[dosbox]\n"
            << "machine = \"svga_s3\"\n"
            << "cpu_cycles = 3000\n"
            << "cpu_cycles_protected = 3000\n"
            << "[launch]\n"
            << "executable = \"GAME.EXE\"\n"
            << "[install]\n"
            << "max_runtime_seconds = 60\n";
    }

    GameCatalog two_games_;
};

TEST_F(DownloadFixture, download_on_a_not_downloaded_tile_starts_the_transfer)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    GameTile* tile = window.grid()->tileFor("delta");
    ASSERT_EQ(tile->state(), TileState::NotDownloaded);

    QTest::mouseClick(tile, Qt::LeftButton);

    EXPECT_EQ(downloader.starts, 1);
    EXPECT_EQ(downloader.last_url.toString(),
              QStringLiteral("https://example.invalid/delta.zip"));
    EXPECT_EQ(downloader.last_destination.filename().string(), "delta.zip");
    EXPECT_EQ(downloader.last_destination.parent_path().filename().string(), "delta");
    EXPECT_EQ(downloader.last_expected_size, 1000u);
    EXPECT_EQ(tile->state(), TileState::Downloading);
}

TEST_F(DownloadFixture, progress_fills_the_downloading_tile)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    GameTile* tile = window.grid()->tileFor("delta");
    QTest::mouseClick(tile, Qt::LeftButton);

    downloader.simulateProgress(500, 1000);

    EXPECT_EQ(tile->progress(), 50);
}

TEST_F(DownloadFixture, a_running_download_shows_the_busy_cursor)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    ASSERT_EQ(QGuiApplication::overrideCursor(), nullptr);

    QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);

    ASSERT_NE(QGuiApplication::overrideCursor(), nullptr);
    EXPECT_EQ(QGuiApplication::overrideCursor()->shape(), Qt::BusyCursor);
    downloader.simulateFinished(QStringLiteral("delta.zip"));
}

TEST_F(DownloadFixture, every_download_ending_restores_the_cursor)
{
    // Finish, failure and cancel are three separate signals; a cursor
    // left pushed on any of them sticks for the rest of the run.
    enum class Ending { Finished, Failed, Cancelled };
    for (const auto ending : {Ending::Finished, Ending::Failed, Ending::Cancelled}) {
        FakeLauncher launcher;
        FakeDownloader downloader;
        ScriptedWindow window(two_games_,
                              assetsDir(),
                              settings(),
                              sizer(),
                              &launcher,
                              &downloader);
        QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);
        ASSERT_NE(QGuiApplication::overrideCursor(), nullptr);

        switch (ending) {
        case Ending::Finished:
            downloader.simulateFinished(QStringLiteral("d.zip"));
            break;
        case Ending::Failed: downloader.simulateFailed(QStringLiteral("404")); break;
        case Ending::Cancelled: downloader.simulateCancelled(); break;
        }

        EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr)
                << "ending " << static_cast<int>(ending);
    }
}

TEST_F(DownloadFixture, a_finished_download_lands_on_downloaded)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    GameTile* tile = window.grid()->tileFor("delta");
    QTest::mouseClick(tile, Qt::LeftButton);

    downloader.simulateFinished(QStringLiteral("delta.zip"));

    EXPECT_EQ(tile->state(), TileState::Downloaded);
}

TEST_F(DownloadFixture, a_failed_download_returns_to_not_downloaded)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    GameTile* tile = window.grid()->tileFor("delta");
    QTest::mouseClick(tile, Qt::LeftButton);

    downloader.simulateFailed(QStringLiteral("server answered 404"));

    EXPECT_EQ(tile->state(), TileState::NotDownloaded);
}

TEST_F(DownloadFixture, clicking_the_downloading_tile_cancels)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    GameTile* tile = window.grid()->tileFor("delta");
    QTest::mouseClick(tile, Qt::LeftButton);

    QTest::mouseClick(tile, Qt::LeftButton);
    EXPECT_EQ(downloader.cancel_calls, 1);

    downloader.simulateCancelled();
    EXPECT_EQ(tile->state(), TileState::NotDownloaded);
}

TEST_F(DownloadFixture, a_second_download_while_one_runs_is_not_started)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader);
    QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);

    QTest::mouseClick(window.grid()->tileFor("epsilon"), Qt::LeftButton);

    EXPECT_EQ(downloader.starts, 1);
    EXPECT_EQ(window.grid()->tileFor("epsilon")->state(), TileState::NotDownloaded);
}

TEST_F(DownloadFixture, without_a_downloader_the_tile_ignores_the_click)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);
    GameTile* tile = window.grid()->tileFor("delta");

    QTest::mouseClick(tile, Qt::LeftButton);

    EXPECT_EQ(tile->state(), TileState::NotDownloaded);
}

TEST_F(IntegrityFixture, an_intact_install_launches_without_an_offer)
{
    writeGammaData(4);
    FakeLauncher launcher;
    ScriptedWindow window(one_game_, assetsDir(), settings(), sizer(), &launcher);

    QTest::mouseClick(window.grid()->tileFor("gamma"), Qt::LeftButton);

    EXPECT_EQ(window.reinstall_offers, 0);
    ASSERT_EQ(launcher.launched_slugs.size(), 1u);
    EXPECT_EQ(launcher.launched_slugs.front(), "gamma");
}

TEST_F(IntegrityFixture, a_damaged_install_offers_reinstall_instead_of_launching)
{
    writeGammaData(2);
    FakeLauncher launcher;
    ScriptedWindow window(one_game_, assetsDir(), settings(), sizer(), &launcher);

    QTest::mouseClick(window.grid()->tileFor("gamma"), Qt::LeftButton);

    EXPECT_EQ(window.reinstall_offers, 1);
    EXPECT_EQ(window.named_reinstall_title, QStringLiteral("Gamma"));
    EXPECT_TRUE(launcher.launched_slugs.empty());
    // A game that will not launch asks no port question.
    EXPECT_EQ(window.port_notices, 0);
}

TEST_F(IntegrityFixture, declining_reinstall_keeps_the_tile_ready)
{
    writeGammaData(2);
    FakeLauncher launcher;
    ScriptedWindow window(one_game_, assetsDir(), settings(), sizer(), &launcher);

    QTest::mouseClick(window.grid()->tileFor("gamma"), Qt::LeftButton);

    EXPECT_EQ(window.grid()->tileFor("gamma")->state(), TileState::Ready);
}

TEST_F(IntegrityFixture, accepting_reinstall_without_an_archive_demotes_to_not_downloaded)
{
    writeGammaData(2);
    FakeLauncher launcher;
    ScriptedWindow window(one_game_, assetsDir(), settings(), sizer(), &launcher);
    window.accept_reinstall = true;

    QTest::mouseClick(window.grid()->tileFor("gamma"), Qt::LeftButton);

    EXPECT_EQ(window.grid()->tileFor("gamma")->state(), TileState::NotDownloaded);
}

TEST_F(IntegrityFixture,
       accepting_reinstall_with_an_archive_on_disk_demotes_to_downloaded)
{
    writeGammaData(2);
    std::filesystem::create_directories(cache_ / "downloads" / "gamma");
    {
        std::ofstream out(cache_ / "downloads" / "gamma" / "gamma.zip");
        out << "zip";
    }
    FakeLauncher launcher;
    ScriptedWindow window(one_game_, assetsDir(), settings(), sizer(), &launcher);
    window.accept_reinstall = true;

    QTest::mouseClick(window.grid()->tileFor("gamma"), Qt::LeftButton);

    EXPECT_EQ(window.grid()->tileFor("gamma")->state(), TileState::Downloaded);
}

class FakeConnectivity : public Connectivity {
public:
    FakeConnectivity() : Connectivity(nullptr) {}

    bool isOnline() const override { return online; }

    void setOnline(bool state)
    {
        if (online == state) {
            return;
        }
        online = state;
        emit onlineChanged(state);
    }

    bool online = true;
};

class ConnectivityFixture : public DownloadFixture {};

TEST_F(ConnectivityFixture, offline_at_startup_sets_not_downloaded_tiles_to_offline)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;
    connectivity.online = false;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);

    EXPECT_EQ(window.grid()->tileFor("delta")->state(), TileState::OfflineNotDownloaded);
    EXPECT_EQ(window.grid()->tileFor("epsilon")->state(),
              TileState::OfflineNotDownloaded);
}

TEST_F(ConnectivityFixture, going_offline_demotes_not_downloaded_tiles)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);
    ASSERT_EQ(window.grid()->tileFor("delta")->state(), TileState::NotDownloaded);

    connectivity.setOnline(false);

    EXPECT_EQ(window.grid()->tileFor("delta")->state(), TileState::OfflineNotDownloaded);
    EXPECT_EQ(window.grid()->tileFor("epsilon")->state(),
              TileState::OfflineNotDownloaded);
}

TEST_F(ConnectivityFixture, coming_back_online_restores_not_downloaded_tiles)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);

    connectivity.setOnline(false);
    ASSERT_EQ(window.grid()->tileFor("delta")->state(), TileState::OfflineNotDownloaded);

    connectivity.setOnline(true);

    EXPECT_EQ(window.grid()->tileFor("delta")->state(), TileState::NotDownloaded);
    EXPECT_EQ(window.grid()->tileFor("epsilon")->state(), TileState::NotDownloaded);
}

TEST_F(ConnectivityFixture, an_installed_game_is_unaffected_by_going_offline)
{
    std::filesystem::create_directories(cache_ / "installs" / "delta");
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);
    ASSERT_EQ(window.grid()->tileFor("delta")->state(), TileState::Ready);

    connectivity.setOnline(false);

    EXPECT_EQ(window.grid()->tileFor("delta")->state(), TileState::Ready);
    EXPECT_EQ(window.grid()->tileFor("epsilon")->state(),
              TileState::OfflineNotDownloaded);
}

TEST_F(ConnectivityFixture, download_is_blocked_while_offline)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;
    connectivity.online = false;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);

    connectivity.setOnline(true);
    QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);
    ASSERT_EQ(downloader.starts, 1);

    downloader.simulateFailed("test");
    connectivity.setOnline(false);
    connectivity.setOnline(true);

    QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);
    connectivity.setOnline(false);
    EXPECT_EQ(downloader.starts, 2);
}

TEST_F(ConnectivityFixture, a_failed_download_while_offline_lands_on_offline_state)
{
    FakeLauncher launcher;
    FakeDownloader downloader;
    FakeConnectivity connectivity;

    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          &downloader,
                          &connectivity);

    QTest::mouseClick(window.grid()->tileFor("delta"), Qt::LeftButton);
    ASSERT_EQ(window.grid()->tileFor("delta")->state(), TileState::Downloading);

    connectivity.setOnline(false);
    downloader.simulateFailed("connection lost");

    EXPECT_EQ(window.grid()->tileFor("delta")->state(), TileState::OfflineNotDownloaded);
}

// Records what the window asked for; the tests play the signals back.
class FakeInstallRunner : public InstallRunner {
public:
    FakeInstallRunner()
            : InstallRunner("/nonexistent/engine", "/nonexistent/cache",
                            "/nonexistent/games", dummyExtractor())
    {}

    bool startInstall(const GameDefinition& game, std::string& error) override
    {
        if (refuse_install) {
            error = "refused by the fake";
            return false;
        }
        started_slugs.push_back(game.slug());
        fake_running_slug = QString::fromStdString(game.slug());
        return true;
    }

    bool isRunning() const override { return !fake_running_slug.isEmpty(); }
    QString installingSlug() const override { return fake_running_slug; }

    void simulateProgress(const QString& slug, int percent)
    {
        emit progressChanged(slug, percent);
    }
    void simulateSucceeded(const QString& slug)
    {
        fake_running_slug.clear();
        emit succeeded(slug);
    }
    void simulateFailed(const QString& slug, const QString& reason)
    {
        fake_running_slug.clear();
        emit failed(slug, reason);
    }

    std::vector<std::string> started_slugs;
    bool refuse_install = false;
    QString fake_running_slug;

private:
    static const ArchiveExtractor& dummyExtractor()
    {
        static const ArchiveExtractor kExtractor;
        return kExtractor;
    }
};

// Like SwitchFixture, but alpha starts from the archive station: a
// downloaded file on disk and no install directory.
class InstallFixture : public SwitchFixture {
protected:
    void SetUp() override
    {
        SwitchFixture::SetUp();
        std::filesystem::remove_all(cache_ / "installs" / "alpha");
        std::filesystem::create_directories(cache_ / "downloads" / "alpha");
        std::ofstream(cache_ / "downloads" / "alpha" / "alpha.zip") << "archive";
    }
};

TEST_F(InstallFixture, a_downloaded_archive_boots_the_tile_at_downloaded)
{
    FakeLauncher launcher;
    ScriptedWindow window(two_games_, assetsDir(), settings(), sizer(), &launcher);

    EXPECT_EQ(window.grid()->tileFor("alpha")->state(), TileState::Downloaded);
    EXPECT_EQ(window.grid()->tileFor("beta")->state(), TileState::Ready);
}

TEST_F(InstallFixture, a_running_install_locks_every_other_tile)
{
    // A second engine would collide on the shared port, so the grid says
    // so instead of swallowing the click (aug-ep52).
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_TRUE(window.grid()->tileFor("beta")->isLocked());
    EXPECT_FALSE(window.grid()->tileFor("alpha")->isLocked());
}

TEST_F(InstallFixture, a_succeeded_install_unlocks_the_tiles)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    runner.simulateSucceeded("alpha");

    EXPECT_FALSE(window.grid()->tileFor("beta")->isLocked());
}

TEST_F(InstallFixture, a_failed_install_unlocks_the_tiles)
{
    // The lock must lift on both exits or the grid stays dead.
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    runner.simulateFailed("alpha", "boom");

    EXPECT_FALSE(window.grid()->tileFor("beta")->isLocked());
}

TEST_F(InstallFixture, a_locked_tile_ignores_its_click)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);
    runner.started_slugs.clear();

    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    EXPECT_TRUE(runner.started_slugs.empty());
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(InstallFixture, clicking_a_downloaded_tile_starts_the_install)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);

    EXPECT_EQ(runner.started_slugs, std::vector<std::string>{"alpha"});
    EXPECT_EQ(tile->state(), TileState::Installing);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(InstallFixture, install_progress_reaches_the_tile)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);
    runner.simulateProgress("alpha", 50);

    EXPECT_EQ(tile->progress(), 50);
}

TEST_F(InstallFixture, a_successful_install_turns_ready_and_launches)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);
    runner.simulateSucceeded("alpha");

    EXPECT_EQ(tile->state(), TileState::Ready);
    EXPECT_EQ(launcher.launched_slugs, std::vector<std::string>{"alpha"});
}

TEST_F(InstallFixture, a_failed_install_returns_the_tile_to_downloaded)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);
    runner.simulateFailed("alpha", "verification failed");

    EXPECT_EQ(tile->state(), TileState::Downloaded);
    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(InstallFixture, a_refused_install_leaves_the_tile_downloaded)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    runner.refuse_install = true;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    GameTile* tile = window.grid()->tileFor("alpha");

    QTest::mouseClick(tile, Qt::LeftButton);

    EXPECT_EQ(tile->state(), TileState::Downloaded);
}

TEST_F(InstallFixture, no_launch_starts_beside_a_running_install)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    runner.fake_running_slug = QStringLiteral("alpha");
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);

    QTest::mouseClick(window.grid()->tileFor("beta"), Qt::LeftButton);

    EXPECT_TRUE(launcher.launched_slugs.empty());
}

TEST_F(InstallFixture, the_port_notice_gates_the_install_too)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    window.accept_port_notice = false;

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);

    EXPECT_EQ(window.port_notices, 1);
    EXPECT_TRUE(runner.started_slugs.empty());
    EXPECT_EQ(window.grid()->tileFor("alpha")->state(), TileState::Downloaded);
}

TEST_F(InstallFixture, an_install_clicked_beside_a_running_game_waits_for_its_exit)
{
    FakeLauncher launcher;
    FakeInstallRunner runner;
    ScriptedWindow window(two_games_,
                          assetsDir(),
                          settings(),
                          sizer(),
                          &launcher,
                          nullptr,
                          nullptr,
                          &runner);
    window.accept_switch = true;
    launcher.fake_running_slug = QStringLiteral("beta");
    launcher.simulateStarted(QStringLiteral("beta"));

    QTest::mouseClick(window.grid()->tileFor("alpha"), Qt::LeftButton);
    EXPECT_EQ(launcher.stop_calls, 1);
    EXPECT_TRUE(runner.started_slugs.empty());

    launcher.fake_running_slug.clear();
    launcher.simulateEnded(QStringLiteral("beta"));

    EXPECT_EQ(runner.started_slugs, std::vector<std::string>{"alpha"});
    EXPECT_EQ(window.grid()->tileFor("alpha")->state(), TileState::Installing);
}

} // namespace
} // namespace showroom
