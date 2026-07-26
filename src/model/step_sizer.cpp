// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/step_sizer.h"

#include <algorithm>
#include <cstdlib>

namespace showroom {
namespace {

bool isUsableChrome(const GridChrome& chrome)
{
    return chrome.columns > 0 && chrome.rows > 0 && chrome.sidebar_width_px >= 0
        && chrome.gap_px >= 0 && chrome.padding_px >= 0 && chrome.frame_height_px >= 0;
}

} // namespace

StepSizer::StepSizer(const GridChrome& chrome, int available_width_px,
                     int available_height_px)
        : chrome_(chrome),
          available_width_px_(available_width_px),
          available_height_px_(available_height_px)
{
    if (!isUsableChrome(chrome_) || available_width_px_ <= 0
        || available_height_px_ <= 0) {
        return;
    }

    for (int width = kMinTileWidthPx; width <= kNativeScreenshotWidthPx;
         width += kTileStepPx) {
        if (fitsScreen(width)) {
            tile_widths_.push_back(width);
        }
    }
}

bool StepSizer::fitsScreen(int tile_width_px) const
{
    const WindowSize size = windowSizeFor(tile_width_px);
    return size.width_px <= available_width_px_
        && size.height_px + chrome_.frame_height_px <= available_height_px_;
}

int StepSizer::defaultTileWidth() const
{
    if (tile_widths_.empty()) {
        return kMinTileWidthPx;
    }

    const auto found = std::find(tile_widths_.begin(),
                                 tile_widths_.end(),
                                 kDefaultTileWidthPx);
    return found != tile_widths_.end() ? *found : tile_widths_.back();
}

int StepSizer::nextTileWidth(int from_px) const
{
    if (tile_widths_.empty()) {
        return kMinTileWidthPx;
    }

    const auto found = std::upper_bound(tile_widths_.begin(),
                                        tile_widths_.end(),
                                        from_px);
    return found != tile_widths_.end() ? *found : tile_widths_.back();
}

int StepSizer::previousTileWidth(int from_px) const
{
    if (tile_widths_.empty()) {
        return kMinTileWidthPx;
    }

    const auto found = std::lower_bound(tile_widths_.begin(),
                                        tile_widths_.end(),
                                        from_px);
    return found != tile_widths_.begin() ? *(found - 1) : tile_widths_.front();
}

int StepSizer::snapToStep(int desired_tile_width_px) const
{
    if (tile_widths_.empty()) {
        return kMinTileWidthPx;
    }

    int nearest = tile_widths_.front();
    int nearest_distance = std::abs(desired_tile_width_px - nearest);
    for (const int width : tile_widths_) {
        const int distance = std::abs(desired_tile_width_px - width);
        // Not strictly less: an exact tie takes the larger step, since
        // the sequence is ascending and the later one wins.
        if (distance <= nearest_distance) {
            nearest = width;
            nearest_distance = distance;
        }
    }
    return nearest;
}

int StepSizer::tileWidthForWindowSize(int window_width_px, int window_height_px) const
{
    if (tile_widths_.empty()) {
        return kMinTileWidthPx;
    }

    int best = tile_widths_.front();
    for (const int width : tile_widths_) {
        const WindowSize size = windowSizeFor(width);
        if (size.width_px > window_width_px || size.height_px > window_height_px) {
            break;
        }
        best = width;
    }
    return best;
}

WindowSize StepSizer::windowSizeFor(int tile_width_px) const
{
    const int gaps = std::max(chrome_.columns - 1, 0) * chrome_.gap_px;
    const int row_gaps = std::max(chrome_.rows - 1, 0) * chrome_.gap_px;

    WindowSize size;
    size.width_px = chrome_.sidebar_width_px + 2 * chrome_.padding_px
                  + chrome_.columns * tile_width_px + gaps;
    size.height_px = 2 * chrome_.padding_px + chrome_.rows * tileHeightFor(tile_width_px)
                   + row_gaps;
    return size;
}

int StepSizer::tileHeightFor(int tile_width_px)
{
    return tile_width_px * 3 / 4;
}

} // namespace showroom
