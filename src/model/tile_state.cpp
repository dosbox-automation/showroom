// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/tile_state.h"

namespace showroom {

bool isLegalTransition(TileState from, TileState to)
{
    if (from == to) {
        return false;
    }

    switch (from) {
    case TileState::NotDownloaded:
        return to == TileState::Downloading || to == TileState::OfflineNotDownloaded;

    // Cancel and failure are the same move: the archive is not there, so
    // the tile goes back to offering the download.
    case TileState::Downloading:
        return to == TileState::Downloaded || to == TileState::NotDownloaded;

    // The download is verified before it counts, and a rejected archive
    // is discarded rather than kept around to fail the install later.
    case TileState::Downloaded:
        return to == TileState::Installing || to == TileState::NotDownloaded;

    // A failed install keeps the archive so the retry does not download
    // it a second time.
    case TileState::Installing:
        return to == TileState::Ready || to == TileState::Downloaded;

    // A damaged install rolls back for reinstallation: to the kept archive
    // when one is on disk, otherwise all the way to the download button.
    case TileState::Ready:
        return to == TileState::Running || to == TileState::Downloaded
            || to == TileState::NotDownloaded;

    case TileState::Running: return to == TileState::Ready;

    case TileState::OfflineNotDownloaded: return to == TileState::NotDownloaded;

    // The definition carries no recipe. Nothing that happens at runtime
    // changes that, in either direction.
    case TileState::NoRecipe: return false;
    }

    return false;
}

TileAction actionFor(TileState state)
{
    switch (state) {
    case TileState::NotDownloaded: return TileAction::Download;

    // Downloaded but not installed still shows Play: the click installs
    // first and then launches, which is one user intent, not two.
    case TileState::Downloaded:
    case TileState::Ready: return TileAction::Play;

    case TileState::Running: return TileAction::Stop;

    case TileState::Downloading:
    case TileState::Installing:
    case TileState::OfflineNotDownloaded:
    case TileState::NoRecipe: return TileAction::None;
    }

    return TileAction::None;
}

TileTone toneFor(TileState state)
{
    switch (state) {
    case TileState::NotDownloaded:
    case TileState::Downloaded: return TileTone::Idle;

    case TileState::Downloading:
    case TileState::Installing: return TileTone::Working;

    case TileState::Ready:
    case TileState::Running: return TileTone::Ready;

    case TileState::OfflineNotDownloaded:
    case TileState::NoRecipe: return TileTone::Disabled;
    }

    return TileTone::Idle;
}

bool showsProgress(TileState state)
{
    return toneFor(state) == TileTone::Working;
}

const char* tileStateName(TileState state)
{
    switch (state) {
    case TileState::NotDownloaded: return "not_downloaded";
    case TileState::Downloading: return "downloading";
    case TileState::Downloaded: return "downloaded";
    case TileState::Installing: return "installing";
    case TileState::Ready: return "ready";
    case TileState::Running: return "running";
    case TileState::OfflineNotDownloaded: return "offline_not_downloaded";
    case TileState::NoRecipe: return "no_recipe";
    }

    return "unknown";
}

} // namespace showroom
