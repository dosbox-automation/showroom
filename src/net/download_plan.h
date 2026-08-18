// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_DOWNLOAD_PLAN_H
#define SHOWROOM_NET_DOWNLOAD_PLAN_H

#include "model/game_definition.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace showroom {

struct DownloadPlan {
    std::string url;
    std::string filename;
    std::optional<std::uint64_t> size;
    // Travels with the plan because mirrors may install differently
    // than the primary (aug-ctpt).
    std::optional<InstallType> install_type;
};

// One plan per usable source, in the parser's role order, so primary
// leads and mirrors follow. A source is skipped when neither its
// declared filename nor its URL basename is a safe path component - a
// bad name must not reach the filesystem.
std::vector<DownloadPlan> downloadPlansFor(const GameDefinition& game);

// The lead plan, for callers that only ever start a chain.
std::optional<DownloadPlan> downloadPlanFor(const GameDefinition& game);

// The first plan, in role order, whose archive exists in the given
// directory: the installer must act on the source that actually
// landed, not on the one it would have preferred.
std::optional<DownloadPlan> archivePlanOnDisk(const GameDefinition& game,
                                              const std::filesystem::path& downloads_dir);

} // namespace showroom

#endif // SHOWROOM_NET_DOWNLOAD_PLAN_H
