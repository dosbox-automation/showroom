// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/install_check.h"

#include "imported/log.h"

#include <system_error>

namespace showroom {

std::vector<std::string> installDamage(const GameDefinition& game,
                                       const std::filesystem::path& install_dir)
{
    std::vector<std::string> damage;
    for (const ExpectedFile& expected : game.install().expected_files) {
        // The path is validated safe-relative at parse time; a definition
        // never arrives here half-constructed.
        const auto path = install_dir / expected.path;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            damage.push_back(expected.path + ": missing");
            continue;
        }
        if (!expected.size.has_value()) {
            continue;
        }
        const auto actual = std::filesystem::file_size(path, ec);
        if (ec) {
            damage.push_back(expected.path + ": unreadable: " + ec.message());
            continue;
        }
        if (actual != *expected.size) {
            damage.push_back(expected.path + ": " + std::to_string(actual)
                             + " bytes, expected " + std::to_string(*expected.size));
        }
    }
    return damage;
}

std::vector<std::string> verifyInstall(const GameDefinition& game,
                                       const std::filesystem::path& install_dir)
{
    if (game.install().expected_files.empty()) {
        log_warn("install_check",
                 "%s: no expected files declared, install result unverifiable",
                 game.slug().c_str());
        return {};
    }
    return installDamage(game, install_dir);
}

} // namespace showroom
