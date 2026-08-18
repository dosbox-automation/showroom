// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/tile_grid.h"

#include "model/game_catalog.h"
#include "model/step_sizer.h"

#include <gtest/gtest.h>

#include <QLabel>

namespace showroom {
namespace {

std::filesystem::path gamesDir()
{
    return std::filesystem::path(SHOWROOM_SOURCE_ASSETS_DIR) / "games";
}

// The state the da4 tarball shipped in: no games directory at all, so
// the window opened onto a black void with no explanation (aug-rrwq).
TEST(TileGridEmptyState, an_empty_catalog_shows_a_message_instead_of_a_void)
{
    const std::filesystem::path missing("/nonexistent/showroom-games");
    const GameCatalog empty = GameCatalog::loadFromDirectory(missing);
    ASSERT_EQ(empty.size(), 0u);

    const TileGrid grid(empty, missing, GridChrome{});

    const auto* message = grid.findChild<QLabel*>();
    ASSERT_NE(message, nullptr);
    EXPECT_TRUE(message->text().contains(QStringLiteral("No games")))
            << message->text().toStdString();
    EXPECT_TRUE(message->text().contains(QStringLiteral("showroom-games")))
            << message->text().toStdString();
}

TEST(TileGridEmptyState, the_message_fills_the_grid_area_of_the_current_step)
{
    const std::filesystem::path missing("/nonexistent/showroom-games");
    const GameCatalog empty = GameCatalog::loadFromDirectory(missing);
    const GridChrome chrome;

    TileGrid grid(empty, missing, chrome);
    grid.setTileWidth(240);

    const auto* message = grid.findChild<QLabel*>();
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->width(),
              chrome.columns * 240 + (chrome.columns - 1) * chrome.gap_px);
    EXPECT_EQ(message->height(),
              chrome.rows * StepSizer::tileHeightFor(240)
                      + (chrome.rows - 1) * chrome.gap_px);
}

TEST(TileGridEmptyState, a_populated_catalog_shows_no_message)
{
    const GameCatalog games = GameCatalog::loadFromDirectory(gamesDir());
    ASSERT_GT(games.size(), 0u);

    const TileGrid grid(games, gamesDir(), GridChrome{});

    EXPECT_EQ(grid.findChild<QLabel*>(), nullptr);
}

} // namespace
} // namespace showroom
