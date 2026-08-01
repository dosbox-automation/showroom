// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_CONF_WRITER_H
#define SHOWROOM_ENGINE_CONF_WRITER_H

#include "model/game_definition.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace showroom {

// Off the engine default 8386, which a developer instance on the same
// box usually owns. Written into the run conf and dialed by ApiClient.
inline constexpr std::uint16_t kShowroomEnginePort = 8686;

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

    // Install mode: mounts only, no launch, no exit - the recipe types
    // the installer invocation and controls shutdown. C: is a staging
    // dir under the extracts dir because without a primary config the
    // conf anchor is the engine's only allowed mount root; the runner
    // moves the staged result into installs/<slug> after verification.
    static std::optional<std::string> renderInstallConf(
            const GameDefinition& game, const std::filesystem::path& extracts_dir,
            std::string& error);

    // Lands at <extracts_dir>/install.conf: the conf's directory is an
    // image root, which is what lets the recipe's bare-name drive_swap
    // pass mount policy.
    static std::optional<std::filesystem::path> writeInstallConf(
            const GameDefinition& game, const std::filesystem::path& extracts_dir,
            std::string& error);

    // The staging dir renderInstallConf mounts as C:.
    static std::filesystem::path installStagingDir(
            const std::filesystem::path& extracts_dir)
    {
        return extracts_dir / "installs";
    }
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_CONF_WRITER_H
