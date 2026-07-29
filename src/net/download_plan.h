// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_DOWNLOAD_PLAN_H
#define SHOWROOM_NET_DOWNLOAD_PLAN_H

#include "model/game_definition.h"

#include <cstdint>
#include <optional>
#include <string>

namespace showroom {

struct DownloadPlan {
    std::string url;
    std::string filename;
    std::optional<std::uint64_t> size;
};

// First source after the parser's role sort, so primary wins. No plan
// when neither the declared filename nor the URL basename is a safe
// path component - a bad name must not reach the filesystem.
std::optional<DownloadPlan> downloadPlanFor(const GameDefinition& game);

} // namespace showroom

#endif // SHOWROOM_NET_DOWNLOAD_PLAN_H
