// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/game_tile.h"

#include "model/game_catalog.h"
#include "model/step_sizer.h"
#include "ui/theme.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPixmap>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <memory>

namespace showroom {
namespace {

// The bundled assets, the same ones the application reads. Rendering a
// tile against real screenshots is the point: a fixture image would not
// catch a capture that is the wrong shape.
std::filesystem::path gamesDir()
{
    return std::filesystem::path(SHOWROOM_SOURCE_ASSETS_DIR) / "games";
}

const GameCatalog& catalog()
{
    static const GameCatalog kLoaded = GameCatalog::loadFromDirectory(gamesDir());
    return kLoaded;
}

const GameDefinition& definitionFor(const char* slug)
{
    const GameDefinition* found = catalog().find(slug);
    EXPECT_NE(found, nullptr) << "no bundled definition for " << slug;
    return *found;
}

std::unique_ptr<GameTile> makeTile(const char* slug, int width_px = 240)
{
    const GameDefinition& definition = definitionFor(slug);
    auto tile = std::make_unique<GameTile>(definition, gamesDir() / definition.slug());
    tile->setTileWidth(width_px);
    return tile;
}

QImage renderTile(GameTile& tile)
{
    QPixmap canvas(tile.size());
    canvas.fill(Qt::black);
    // Without DrawWindowBackground: the default flags paint the palette
    // background over everything and every pixel check becomes a check
    // on the palette.
    tile.render(&canvas, QPoint(), QRegion(), QWidget::DrawChildren);
    return canvas.toImage();
}

TEST(GameTileState, a_game_with_a_launch_recipe_starts_ready_to_download)
{
    EXPECT_EQ(makeTile("doom")->state(), TileState::NotDownloaded);
}

TEST(GameTileState, a_game_without_one_starts_in_the_no_recipe_state)
{
    // Fifteen of the sixteen are in this state today, so it is the one
    // the grid shows most on a first run.
    int without_recipe = 0;
    for (const GameDefinition& definition : catalog()) {
        if (!definition.isLaunchable()) {
            const GameTile tile(definition, gamesDir() / definition.slug());
            EXPECT_EQ(tile.state(), TileState::NoRecipe) << definition.slug();
            ++without_recipe;
        }
    }
    EXPECT_GT(without_recipe, 0) << "no unlaunchable game left to check the state with";
}

TEST(GameTileGeometry, the_tile_is_four_by_three_at_every_step)
{
    for (const int width : {160, 240, 400, 640}) {
        const auto tile = makeTile("doom", width);
        EXPECT_EQ(tile->width(), width);
        EXPECT_EQ(tile->height(), StepSizer::tileHeightFor(width));
    }
}

TEST(GameTilePainting, a_disabled_state_desaturates_the_screenshot)
{
    const auto tile = makeTile("doom");

    tile->setState(TileState::NoRecipe);
    const QImage grey = renderTile(*tile);

    tile->setState(TileState::NotDownloaded);
    const QImage colour = renderTile(*tile);

    // How far apart the channels are, not HSV saturation: saturation is
    // a ratio, so on the near-black pixels a scrimmed tile is mostly
    // made of, three units of difference reads as a third of full
    // colour. The spread stays honest wherever it is measured.
    const auto widest_channel_spread = [](const QImage& image) {
        int widest = 0;
        for (int y = 4; y < image.height() - theme::kLegendHeightPx; y += 3) {
            for (int x = 4; x < image.width() - 4; x += 3) {
                const QColor pixel = image.pixelColor(x, y);
                const int high = std::max({pixel.red(), pixel.green(), pixel.blue()});
                const int low = std::min({pixel.red(), pixel.green(), pixel.blue()});
                widest = std::max(widest, high - low);
            }
        }
        return widest;
    };

    // The scrim is very slightly blue, so a grey tile still has a couple
    // of units between its channels.
    EXPECT_LT(widest_channel_spread(grey), 12)
            << "the disabled tile still has colour in it";
    EXPECT_GT(widest_channel_spread(colour), 60) << "the normal tile lost its colour";
}

TEST(GameTilePainting, hovering_swaps_the_title_shot_for_the_gameplay_shot)
{
    const auto tile = makeTile("doom");
    const QImage resting = renderTile(*tile);

    QEvent enter(QEvent::Enter);
    QApplication::sendEvent(tile.get(), &enter);
    const QImage hovered = renderTile(*tile);

    EXPECT_NE(resting, hovered);

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(tile.get(), &leave);
    EXPECT_EQ(renderTile(*tile), resting);
}

TEST(GameTilePainting, a_working_state_draws_its_progress_across_the_bottom)
{
    const auto tile = makeTile("doom");
    tile->setState(TileState::NotDownloaded);
    tile->setState(TileState::Downloading);

    tile->setProgress(10);
    const QImage early = renderTile(*tile);
    tile->setProgress(90);
    const QImage late = renderTile(*tile);

    // Three quarters of the way along the bottom row: filled at 90 per
    // cent, still track at 10.
    const int y = tile->height() - 2;
    const int x = tile->width() * 3 / 4;
    EXPECT_NE(early.pixelColor(x, y), late.pixelColor(x, y));
}

TEST(GameTileInput, clicking_a_tile_that_offers_an_action_names_its_game)
{
    const auto tile = makeTile("doom");
    QSignalSpy spy(tile.get(), &GameTile::actionTriggered);

    QTest::mouseClick(tile.get(),
                      Qt::LeftButton,
                      Qt::NoModifier,
                      QPoint(tile->width() / 2, tile->height() / 3));

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("doom"));
}

TEST(GameTileInput, clicking_a_tile_with_no_recipe_does_nothing)
{
    const auto tile = makeTile("doom");
    tile->setState(TileState::NoRecipe);

    QSignalSpy spy(tile.get(), &GameTile::actionTriggered);
    QTest::mouseClick(tile.get(),
                      Qt::LeftButton,
                      Qt::NoModifier,
                      QPoint(tile->width() / 2, tile->height() / 3));

    EXPECT_EQ(spy.count(), 0);
}

TEST(GameTileAssets, every_bundled_game_has_both_screenshots_on_disk)
{
    // The tile survives a missing capture by leaving the image empty,
    // which is the right behaviour and a terrible thing to ship. This
    // is the check that says nothing is missing.
    for (const GameDefinition& definition : catalog()) {
        const std::filesystem::path dir = gamesDir() / definition.slug();
        EXPECT_TRUE(std::filesystem::exists(dir / definition.screenshots().title))
                << definition.slug() << " title";
        EXPECT_TRUE(std::filesystem::exists(dir / definition.screenshots().gameplay))
                << definition.slug() << " gameplay";
    }
}

} // namespace
} // namespace showroom
