// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/step_sizer.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace showroom {
namespace {

// The chrome from the mockup's own CSS (DOS Launcher.dc.html): a 78px
// sidebar, 22px of padding round the grid and 16px between tiles, in a
// 1200px-wide card. showroom-mockup-2026-07-18.png renders that at 0.93
// scale, so its measured pixels are all a shade smaller.
GridChrome defaultChrome()
{
    return GridChrome{};
}

TEST(StepSizerSteps, the_steps_are_ascending_multiples_of_the_step_size)
{
    const StepSizer sizer(defaultChrome(), 1920, 1080);
    const std::vector<int>& widths = sizer.tileWidths();

    ASSERT_FALSE(widths.empty());
    std::set<int> seen;
    int previous = 0;
    for (const int width : widths) {
        EXPECT_GT(width, previous) << "widths must ascend strictly";
        EXPECT_GE(width, kMinTileWidthPx);
        EXPECT_EQ((width - kMinTileWidthPx) % kTileStepPx, 0);
        EXPECT_TRUE(seen.insert(width).second) << "duplicate width " << width;
        previous = width;
    }
}

TEST(StepSizerSteps, every_offered_step_produces_a_window_that_fits_the_screen)
{
    // The point of filtering: whatever comes back must be selectable
    // without pushing the window off the display.
    for (const auto& screen : {std::pair{1366, 768},
                               std::pair{1920, 1080},
                               std::pair{2560, 1440},
                               std::pair{3840, 2160}}) {
        const StepSizer sizer(defaultChrome(), screen.first, screen.second);
        for (const int width : sizer.tileWidths()) {
            const WindowSize size = sizer.windowSizeFor(width);
            EXPECT_LE(size.width_px, screen.first) << "tile " << width;
            EXPECT_LE(size.height_px + defaultChrome().frame_height_px, screen.second)
                    << "tile " << width;
        }
    }
}

TEST(StepSizerSteps, a_1366x768_laptop_gets_the_minimum_and_nothing_that_overflows)
{
    const StepSizer sizer(defaultChrome(), 1366, 768);
    const std::vector<int>& widths = sizer.tileWidths();

    ASSERT_FALSE(widths.empty());
    EXPECT_EQ(widths.front(), kMinTileWidthPx);

    // Four rows of 4:3 tiles make height the binding constraint on this
    // screen long before width is: 200px tiles need 682px of window
    // height, 240px tiles need 802px, and only 720px are available once
    // the title bar is paid for.
    EXPECT_EQ(widths.back(), 200);
}

TEST(StepSizerSteps, height_binds_before_width_on_a_1080p_screen)
{
    const StepSizer sizer(defaultChrome(), 1920, 1080);

    // Width alone would allow 440. It is the four rows that stop at 280.
    EXPECT_EQ(sizer.tileWidths().back(), 280);
    EXPECT_LE(sizer.windowSizeFor(280).height_px + defaultChrome().frame_height_px, 1080);
}

TEST(StepSizerSteps, a_taller_screen_offers_more_steps_than_a_wider_one)
{
    const StepSizer wide(defaultChrome(), 2560, 1080);
    const StepSizer tall(defaultChrome(), 1920, 1440);

    EXPECT_EQ(wide.tileWidths().back(), 280);
    EXPECT_GT(tall.tileWidths().back(), wide.tileWidths().back());
}

TEST(StepSizerSteps, no_step_upscales_past_the_native_screenshot_width)
{
    // The bundled captures are 1067px wide. Beyond that a tile only
    // stretches pixels, so the sequence stops even on a wall display.
    const StepSizer sizer(defaultChrome(), 15360, 8640);
    EXPECT_LE(sizer.tileWidths().back(), kNativeScreenshotWidthPx);
}

TEST(StepSizerDefault, the_default_is_240_when_it_fits)
{
    EXPECT_EQ(StepSizer(defaultChrome(), 1920, 1080).defaultTileWidth(),
              kDefaultTileWidthPx);
    EXPECT_EQ(StepSizer(defaultChrome(), 2560, 1440).defaultTileWidth(),
              kDefaultTileWidthPx);
}

TEST(StepSizerDefault, the_default_drops_to_the_largest_step_that_fits)
{
    const StepSizer sizer(defaultChrome(), 1366, 768);
    EXPECT_EQ(sizer.defaultTileWidth(), 200);
}

TEST(StepSizerDefault, a_screen_too_small_for_any_step_still_yields_the_minimum)
{
    // Better a window that overflows a 640x480 display than a window
    // with no tiles in it. The caller is told by an empty step list.
    const StepSizer sizer(defaultChrome(), 640, 480);
    EXPECT_TRUE(sizer.tileWidths().empty());
    EXPECT_EQ(sizer.defaultTileWidth(), kMinTileWidthPx);
}

TEST(StepSizerGeometry, the_window_size_is_the_documented_arithmetic)
{
    const GridChrome chrome = defaultChrome();
    const StepSizer sizer(chrome, 1920, 1080);

    const WindowSize size = sizer.windowSizeFor(240);
    const int expected_width = chrome.sidebar_width_px + 2 * chrome.padding_px
                             + chrome.columns * 240
                             + (chrome.columns - 1) * chrome.gap_px;
    const int expected_height = 2 * chrome.padding_px + chrome.rows * (240 * 3 / 4)
                              + (chrome.rows - 1) * chrome.gap_px;

    EXPECT_EQ(size.width_px, expected_width);
    EXPECT_EQ(size.height_px, expected_height);

    // The mockup is 1114x817 at a 233px tile; the default step lands
    // within a handful of pixels of it.
    EXPECT_NEAR(size.width_px, 1114, 30);
    EXPECT_NEAR(size.height_px, 817, 30);
}

TEST(StepSizerGeometry, tiles_keep_a_4_to_3_ratio)
{
    EXPECT_EQ(StepSizer::tileHeightFor(240), 180);
    EXPECT_EQ(StepSizer::tileHeightFor(160), 120);
    EXPECT_EQ(StepSizer::tileHeightFor(1067), 800);
}

TEST(StepSizerStepping, stepping_up_and_down_stops_at_the_ends)
{
    const StepSizer sizer(defaultChrome(), 1920, 1080);
    ASSERT_EQ(sizer.tileWidths().front(), 160);
    ASSERT_EQ(sizer.tileWidths().back(), 280);

    EXPECT_EQ(sizer.nextTileWidth(160), 200);
    EXPECT_EQ(sizer.previousTileWidth(200), 160);

    EXPECT_EQ(sizer.nextTileWidth(280), 280) << "already the largest";
    EXPECT_EQ(sizer.previousTileWidth(160), 160) << "already the smallest";
}

TEST(StepSizerStepping, stepping_from_a_width_that_is_not_a_step_moves_by_one_step)
{
    // The persisted step can outlive a monitor change, so a value that
    // is no longer offered must not strand the keyboard shortcuts.
    const StepSizer sizer(defaultChrome(), 1920, 1080);

    EXPECT_EQ(sizer.nextTileWidth(217), 240);
    EXPECT_EQ(sizer.previousTileWidth(217), 200);
    EXPECT_EQ(sizer.nextTileWidth(9000), 280);
    EXPECT_EQ(sizer.previousTileWidth(-5), 160);
}

TEST(StepSizerSnapping, snapping_picks_the_nearest_offered_step)
{
    const StepSizer sizer(defaultChrome(), 1920, 1080);

    EXPECT_EQ(sizer.snapToStep(240), 240);
    EXPECT_EQ(sizer.snapToStep(239), 240);
    EXPECT_EQ(sizer.snapToStep(221), 240);
    EXPECT_EQ(sizer.snapToStep(219), 200);
    EXPECT_EQ(sizer.snapToStep(0), 160);
    EXPECT_EQ(sizer.snapToStep(4000), 280);
}

TEST(StepSizerSnapping, a_dragged_window_resolves_to_the_largest_step_that_fits_inside_it)
{
    const StepSizer sizer(defaultChrome(), 1920, 1080);

    const WindowSize at240 = sizer.windowSizeFor(240);
    EXPECT_EQ(sizer.tileWidthForWindowSize(at240.width_px, at240.height_px), 240);

    // One pixel short in either direction and the next step down wins.
    EXPECT_EQ(sizer.tileWidthForWindowSize(at240.width_px - 1, at240.height_px), 200);
    EXPECT_EQ(sizer.tileWidthForWindowSize(at240.width_px, at240.height_px - 1), 200);

    // Smaller than the minimum still answers with the minimum.
    EXPECT_EQ(sizer.tileWidthForWindowSize(10, 10), kMinTileWidthPx);
}

TEST(StepSizerHostileInput, nonsense_screen_geometry_does_not_produce_nonsense_steps)
{
    for (const auto& screen : {std::pair{0, 0},
                               std::pair{-1920, -1080},
                               std::pair{1920, -1},
                               std::pair{-1, 1080}}) {
        const StepSizer sizer(defaultChrome(), screen.first, screen.second);
        EXPECT_TRUE(sizer.tileWidths().empty());
        EXPECT_EQ(sizer.defaultTileWidth(), kMinTileWidthPx);
        EXPECT_EQ(sizer.snapToStep(240), kMinTileWidthPx);
    }
}

TEST(StepSizerHostileInput, a_chrome_with_no_columns_or_rows_is_rejected_not_divided_by)
{
    GridChrome chrome = defaultChrome();
    chrome.columns = 0;
    chrome.rows = 0;

    const StepSizer sizer(chrome, 1920, 1080);
    EXPECT_TRUE(sizer.tileWidths().empty());
    EXPECT_EQ(sizer.defaultTileWidth(), kMinTileWidthPx);
}

TEST(StepSizerHostileInput, negative_chrome_measurements_are_rejected)
{
    GridChrome chrome = defaultChrome();
    chrome.gap_px = -100;

    const StepSizer sizer(chrome, 1920, 1080);
    EXPECT_TRUE(sizer.tileWidths().empty());
}

} // namespace
} // namespace showroom
