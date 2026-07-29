// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/install_check.h"

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

} // namespace showroom
