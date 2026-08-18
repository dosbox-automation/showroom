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

// Real screenshots, not fixtures: a fixture would not catch a capture
// that is the wrong shape.
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
    // Built here, not taken from the catalogue: every bundled game has a
    // recipe now, and the state still has to hold for one that does not.
    std::string error;
    const auto definition = GameDefinition::fromTomlString(R"(slug = "probe"
title = "Probe"
rank = 1
license = "shareware"

[sources.primary]
url = "https://example.invalid/probe.zip"

[dosbox]
machine = "svga_s3"
cpu_cycles = 3000
cpu_cycles_protected = 3000

[launch]
executable = ""

[install]
max_runtime_seconds = 60
)",
                                                           error);
    ASSERT_TRUE(definition) << error;
    ASSERT_FALSE(definition->isLaunchable());

    const GameTile tile(*definition, gamesDir() / "doom");
    EXPECT_EQ(tile.state(), TileState::NoRecipe);
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

    // Channel spread, not HSV saturation: saturation is a ratio, so on
    // near-black pixels three units of difference reads as a third of
    // full colour.
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

TEST(GameTilePainting, a_started_transfer_shows_a_sliver_before_any_progress_arrives)
{
    // The click turns the cursor busy; a bar still empty at that moment
    // reads as nothing having happened. The transfer HAS started, so
    // saying so is honest rather than a fake animation.
    const auto tile = makeTile("doom");
    tile->setState(TileState::NotDownloaded);
    tile->setState(TileState::Downloading);
    tile->setProgress(0);

    // Sampled clear of the 10px corner radius, which eats anything
    // narrower than itself.
    EXPECT_EQ(renderTile(*tile).pixelColor(12, 3), theme::kAmber);
}

TEST(GameTilePainting, a_working_state_draws_its_progress_across_the_top)
{
    const auto tile = makeTile("doom");
    tile->setState(TileState::NotDownloaded);
    tile->setState(TileState::Downloading);

    tile->setProgress(10);
    const QImage early = renderTile(*tile);
    tile->setProgress(90);
    const QImage late = renderTile(*tile);

    // Three quarters of the way along the top row: filled at 90 per
    // cent, still track at 10. The bottom belongs to the legend.
    const int y = 1;
    const int x = tile->width() * 3 / 4;
    EXPECT_NE(early.pixelColor(x, y), late.pixelColor(x, y));
}

TEST(GameTilePainting, a_tile_without_screenshots_names_its_game)
{
    // The da4 tarball shipped without screenshots and every tile was an
    // anonymous dark frame (aug-rrwq). The title travels with the
    // definition, so a missing capture must still name the game.
    std::string error;
    const auto definition = GameDefinition::fromTomlString(R"(slug = "probe"
title = "Probe Game"
rank = 1
license = "shareware"

[sources.primary]
url = "https://example.invalid/probe.zip"

[dosbox]
machine = "svga_s3"
cpu_cycles = 3000
cpu_cycles_protected = 3000

[launch]
executable = "GAME.EXE"

[install]
max_runtime_seconds = 60
)",
                                                           error);
    ASSERT_TRUE(definition) << error;

    GameTile tile(*definition, gamesDir() / "no-such-directory");
    tile.setTileWidth(240);

    // Ink between the progress strip and the legend; without the
    // placeholder that whole region is the uniform background fill.
    // Margins keep the scan clear of the rounded border.
    const QImage image = renderTile(tile);
    bool ink = false;
    for (int y = 14; y < image.height() - theme::kLegendHeightPx && !ink; ++y) {
        for (int x = 14; x < image.width() - 14; ++x) {
            if (image.pixelColor(x, y) != theme::kSidebarBackground) {
                ink = true;
                break;
            }
        }
    }
    EXPECT_TRUE(ink) << "the screenshot-less tile paints nothing but background";
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

TEST(GameTileTooltip, includes_title_year_publisher_and_blurb)
{
    const auto tile = makeTile("doom");
    const QString tip = tile->toolTip();

    EXPECT_TRUE(tip.contains("<nobr><b>DOOM</b>")) << tip.toStdString();
    EXPECT_TRUE(tip.contains("1993")) << tip.toStdString();
    EXPECT_TRUE(tip.contains("id Software")) << tip.toStdString();
    EXPECT_TRUE(tip.contains("shotgun")) << tip.toStdString();
}

TEST(GameTileTooltip, html_escapes_special_characters_in_the_title)
{
    std::string error;
    const auto definition = GameDefinition::fromTomlString(R"(slug = "probe"
title = "Probe <script>"
rank = 1
license = "shareware"
year = 1999
publisher = "Foo & Bar"
blurb = "A \"tricky\" game."

[sources.primary]
url = "https://example.invalid/probe.zip"

[dosbox]
machine = "svga_s3"
cpu_cycles = 3000
cpu_cycles_protected = 3000

[launch]
executable = ""

[install]
max_runtime_seconds = 60
)",
                                                           error);
    ASSERT_TRUE(definition) << error;

    const GameTile tile(*definition, gamesDir() / "doom");
    const QString tip = tile.toolTip();

    EXPECT_TRUE(tip.contains("&lt;script&gt;")) << tip.toStdString();
    EXPECT_TRUE(tip.contains("Foo &amp; Bar")) << tip.toStdString();
    EXPECT_TRUE(tip.contains("&quot;tricky&quot;")) << tip.toStdString();
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
