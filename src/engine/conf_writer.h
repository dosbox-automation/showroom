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
    // dir under the extracts dir; the runner moves the staged result
    // into installs/<slug> after verification. downloaded_type overrides
    // the primary's install type when a mirror of a different type is the
    // source that actually landed (aug-qerw doom.iso).
    static std::optional<std::string> renderInstallConf(
            const GameDefinition& game, const std::filesystem::path& cache_base,
            std::string& error,
            std::optional<InstallType> downloaded_type = std::nullopt);

    // Floppy and exe install confs land in the extracts dir: the
    // conf's directory is an image root, which is what lets the
    // recipe's bare-name drive_swap pass mount policy. Confs for games
    // whose download IS the medium land at the cache base instead - the
    // anchor must cover downloads/, where the medium stays.
    static std::optional<std::filesystem::path> writeInstallConf(
            const GameDefinition& game, const std::filesystem::path& cache_base,
            std::string& error,
            std::optional<InstallType> downloaded_type = std::nullopt);

    // True when the pinned download is itself the install medium - a CD
    // image, or a bare floppy image such as ckeen4's. Such a download
    // mounts where it lies, so there is nothing to extract and the conf
    // anchor moves to the cache base to reach it. downloaded_type as in
    // renderInstallConf.
    static bool downloadIsMedium(const GameDefinition& game,
                                 const std::filesystem::path& cache_base,
                                 std::optional<InstallType> downloaded_type = std::nullopt);

    // Where an archive-based install unpacks; iso installs have no
    // extraction and mount the download directly.
    static std::filesystem::path extractsDir(const std::filesystem::path& cache_base,
                                             const std::string& slug)
    {
        return cache_base / "extracts" / slug;
    }

    // The staging dir renderInstallConf mounts as C:.
    static std::filesystem::path installStagingDir(
            const std::filesystem::path& extracts_dir)
    {
        return extracts_dir / "installs";
    }
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_CONF_WRITER_H
