// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_INSTALL_CHECK_H
#define SHOWROOM_MODEL_INSTALL_CHECK_H

#include "model/game_definition.h"

#include <filesystem>
#include <string>
#include <vector>

namespace showroom {

// Existence and size only, no hashing: hashing a full install on every
// launch is too slow. Save games and config files are excluded by not
// being listed in the definition. Empty result means intact.
std::vector<std::string> installDamage(const GameDefinition& game,
                                       const std::filesystem::path& install_dir);

// Post-install gate: installDamage plus the empty-expectations rule -
// a definition listing no files cannot prove an install worked, so
// that case passes with a logged warning instead of silently.
std::vector<std::string> verifyInstall(const GameDefinition& game,
                                       const std::filesystem::path& install_dir);

// Pre-launch media gate: a CD title whose downloads dir holds no ISO
// cannot boot - the run conf mounts the image. Empty result means
// playable; non-CD titles always pass.
std::vector<std::string> mediaDamage(const GameDefinition& game,
                                     const std::filesystem::path& downloads_dir);

} // namespace showroom

#endif // SHOWROOM_MODEL_INSTALL_CHECK_H
