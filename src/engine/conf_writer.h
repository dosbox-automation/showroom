// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_CONF_WRITER_H
#define SHOWROOM_ENGINE_CONF_WRITER_H

#include "model/game_definition.h"

#include <filesystem>
#include <optional>
#include <string>

namespace showroom {

// The engine treats the conf file's directory as the allowed mount
// root, so the conf goes into the cache base and nowhere else.
class ConfWriter {
public:
    static std::optional<std::string> renderConf(const GameDefinition& game,
                                                 const std::filesystem::path& cache_base,
                                                 std::string& error);

    // Atomic and owner-only; on failure nothing is left behind.
    static std::optional<std::filesystem::path> writeConf(
            const GameDefinition& game, const std::filesystem::path& cache_base,
            std::string& error);
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_CONF_WRITER_H
