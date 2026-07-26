// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_STEP_SIZER_H
#define SHOWROOM_MODEL_STEP_SIZER_H

#include <vector>

namespace showroom {

// Below this a tile's legend text stops being readable.
inline constexpr int kMinTileWidthPx = 160;

// The window grows and shrinks in whole tiles, 40px of tile width at a
// time.
inline constexpr int kTileStepPx = 40;

// Comfortable on the most common screen, and the size the mockup was
// drawn at.
inline constexpr int kDefaultTileWidthPx = 240;

// The bundled screenshots are 1067x800. A tile wider than the capture
// only stretches pixels, so the step sequence stops here however large
// the display is.
inline constexpr int kNativeScreenshotWidthPx = 1067;

// Everything around the tiles that the window has to pay for. The
// defaults are the mockup's own CSS, not measurements off the rendered
// PNG - that image is a 0.93 scale render and every number in it is
// short by a pixel or two.
struct GridChrome {
    int sidebar_width_px = 78;
    int gap_px = 16;

    // Applied on each of the four sides of the tile area.
    int padding_px = 22;

    // Title bar and borders. The window manager adds them outside the
    // size Qt is asked for, so a window sized to the full screen height
    // ends up taller than the screen unless they are reserved here.
    int frame_height_px = 48;

    int columns = 4;
    int rows = 4;
};

struct WindowSize {
    int width_px = 0;
    int height_px = 0;
};

// Which tile widths a given screen can actually hold, and what window
// each of them implies.
//
// Pure arithmetic on plain integers, deliberately: this is the one part
// of the sizing that has to be right, and keeping Qt out of it means the
// tests need no display. The caller passes logical pixels; scaling for a
// HiDPI display is Qt's business, not this class's.
class StepSizer {
public:
    // Geometry that is not usable - a negative screen, no columns, a
    // negative gap - yields no steps at all rather than a guess. Callers
    // fall back on defaultTileWidth().
    StepSizer(const GridChrome& chrome, int available_width_px, int available_height_px);

    // Ascending, no duplicates, empty when nothing fits.
    const std::vector<int>& tileWidths() const { return tile_widths_; }

    const GridChrome& chrome() const { return chrome_; }

    // kDefaultTileWidthPx when the screen holds it, otherwise the largest
    // step that fits. When no step fits at all this still answers with
    // the minimum: a window that overflows a small display is worse than
    // nothing to show, but not by as much as an empty grid.
    int defaultTileWidth() const;

    // The neighbouring step strictly above or below the given width, so
    // a width that is not itself a step - persisted under a different
    // monitor, say - moves by one and not by two. At either end of the
    // sequence they return that end.
    int nextTileWidth(int from_px) const;
    int previousTileWidth(int from_px) const;

    // Nearest offered step, ties going to the larger one. Used while the
    // window is being dragged.
    int snapToStep(int desired_tile_width_px) const;

    // The largest step whose window fits inside the given size. What a
    // resize handler wants: the user drags a window edge and the grid
    // takes the biggest tiles that still fit in what they dragged.
    int tileWidthForWindowSize(int window_width_px, int window_height_px) const;

    WindowSize windowSizeFor(int tile_width_px) const;

    // Tiles are strictly 4:3, image and legend bar together.
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
