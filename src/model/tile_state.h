// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_TILE_STATE_H
#define SHOWROOM_MODEL_TILE_STATE_H

#include <array>

namespace showroom {

// What a tile is currently showing. The eight states of the design
// document, in the order a game passes through them.
//
// Two of them are not stations on that road. OfflineNotDownloaded is
// NotDownloaded with no network behind it, and NoRecipe describes a
// definition that carries no install recipe at all - it never changes
// while the application runs.
enum class TileState {
    NotDownloaded,
    Downloading,
    Downloaded,
    Installing,
    Ready,
    Running,
    OfflineNotDownloaded,
    NoRecipe,
};

inline constexpr std::array<TileState, 8> kAllTileStates = {
        TileState::NotDownloaded,
        TileState::Downloading,
        TileState::Downloaded,
        TileState::Installing,
        TileState::Ready,
        TileState::Running,
        TileState::OfflineNotDownloaded,
        TileState::NoRecipe};

// The button offered on the right of the legend bar.
enum class TileAction { None, Download, Play, Stop };

// The colour family of the legend text and status dot. The concrete
// colours live in the widget; the choice between them does not, so it
// can be tested without a screen.
enum class TileTone { Idle, Ready, Working, Disabled };

// Whether a tile may move from one state to the other. Keeping the table
// here rather than implicit in the widgets means an illegal move is a
// failed assertion in a test rather than a tile that looks wrong.
//
// A state never transitions to itself: nothing has changed, so nothing
// needs redrawing.
bool isLegalTransition(TileState from, TileState to);

TileAction actionFor(TileState state);
TileTone toneFor(TileState state);

// Whether the legend bar fills as a progress indicator.
bool showsProgress(TileState state);

// For log lines. Returns "unknown" for a value outside the enum rather
// than indexing a table with it.
const char* tileStateName(TileState state);

} // namespace showroom

#endif // SHOWROOM_MODEL_TILE_STATE_H
