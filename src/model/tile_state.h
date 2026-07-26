// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_TILE_STATE_H
#define SHOWROOM_MODEL_TILE_STATE_H

#include <array>

namespace showroom {

// The design document's eight states, in the order a game passes through
// them. OfflineNotDownloaded and NoRecipe are not stations on that road.
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

enum class TileAction { None, Download, Play, Stop };

// The concrete colours live in the widget; choosing between them does
// not, so the choice is testable without a screen.
enum class TileTone { Idle, Ready, Working, Disabled };

// Here rather than implicit in the widgets, so an illegal move fails a
// test instead of merely looking wrong. A state never transitions to
// itself.
bool isLegalTransition(TileState from, TileState to);

TileAction actionFor(TileState state);
TileTone toneFor(TileState state);

bool showsProgress(TileState state);

// Returns "unknown" rather than indexing a table with a value outside
// the enum.
const char* tileStateName(TileState state);

} // namespace showroom

#endif // SHOWROOM_MODEL_TILE_STATE_H
