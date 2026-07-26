// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/main_window.h"

#include "app/settings.h"
#include "model/game_catalog.h"
#include "model/step_sizer.h"
#include "ui/game_tile.h"
#include "ui/tile_grid.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <algorithm>
#include <fstream>
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
    // The application side of what the window manager asks for when the
    // title bar X is clicked. Worth its own test because the desktop
    // half of that path depends on a shortcut binding that can simply be
    // missing, and then a manual check proves nothing either way.
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

} // namespace
} // namespace showroom
