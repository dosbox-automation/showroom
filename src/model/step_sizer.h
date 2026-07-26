// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_STEP_SIZER_H
#define SHOWROOM_MODEL_STEP_SIZER_H

#include <vector>

namespace showroom {

// Below this a tile's legend text stops being readable.
inline constexpr int kMinTileWidthPx = 160;

inline constexpr int kTileStepPx = 40;

// Comfortable on the most common screen, and the size the mockup was
// drawn at.
inline constexpr int kDefaultTileWidthPx = 240;

// The bundled screenshots are this wide; past it a tile only stretches
// pixels, so the sequence stops here however large the display is.
inline constexpr int kNativeScreenshotWidthPx = 1067;

// Defaults are the mockup's own CSS, not measurements off the rendered
// PNG - that image is a 0.93 scale render and every number in it is short.
struct GridChrome {
    int sidebar_width_px = 78;
    int gap_px = 16;

    int padding_px = 22;

    // The window manager adds these outside the size Qt is asked for, so
    // a window sized to the full screen height overflows without them.
    int frame_height_px = 48;

    int columns = 4;
    int rows = 4;
};

struct WindowSize {
    int width_px = 0;
    int height_px = 0;
};

// Plain integer arithmetic, so the tests need no display. Widths are
// logical pixels; HiDPI scaling is Qt's business.
class StepSizer {
public:
    // Unusable geometry yields no steps rather than a guess; callers fall
    // back on defaultTileWidth().
    StepSizer(const GridChrome& chrome, int available_width_px, int available_height_px);

    const std::vector<int>& tileWidths() const { return tile_widths_; }

    const GridChrome& chrome() const { return chrome_; }

    // The default when the screen holds it, otherwise the largest step
    // that fits, and the minimum when none does: a window overflowing a
    // small display beats an empty grid.
    int defaultTileWidth() const;

    // The neighbouring step, so a width that is not itself a step - one
    // persisted under a different monitor - moves by one and not by two.
    int nextTileWidth(int from_px) const;
    int previousTileWidth(int from_px) const;

    int snapToStep(int desired_tile_width_px) const;

    // What a resize handler wants: the biggest tiles that still fit in
    // what the user dragged.
    int tileWidthForWindowSize(int window_width_px, int window_height_px) const;

    WindowSize windowSizeFor(int tile_width_px) const;

    static int tileHeightFor(int tile_width_px);

private:
    bool fitsScreen(int tile_width_px) const;

    GridChrome chrome_;
    int available_width_px_ = 0;
    int available_height_px_ = 0;
    std::vector<int> tile_widths_;
};

} // namespace showroom

#endif // SHOWROOM_MODEL_STEP_SIZER_H
