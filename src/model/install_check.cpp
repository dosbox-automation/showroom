// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/install_check.h"

#include "imported/log.h"

#include <algorithm>
#include <cctype>
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

std::vector<std::string> mediaDamage(const GameDefinition& game,
                                     const std::filesystem::path& downloads_dir)
{
    if (!game.wantsCdDrive()) {
        return {};
    }
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(downloads_dir, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext == ".iso") {
            return {};
        }
    }
    return {"no ISO image in " + downloads_dir.string()};
}

} // namespace showroom
