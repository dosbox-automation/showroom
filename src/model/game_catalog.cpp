// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/game_catalog.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace showroom {
namespace {

int compare_ignoring_case(std::string_view a, std::string_view b)
{
    const auto common = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < common; ++i) {
        const auto left = std::tolower(static_cast<unsigned char>(a[i]));
        const auto right = std::tolower(static_cast<unsigned char>(b[i]));
        if (left != right) {
            return left < right ? -1 : 1;
        }
    }
    if (a.size() == b.size()) {
        return 0;
    }
    return a.size() < b.size() ? -1 : 1;
}

}  // namespace

bool title_precedes(const GameDefinition& a, const GameDefinition& b)
{
    const auto by_title = compare_ignoring_case(a.title(), b.title());
    if (by_title != 0) {
        return by_title < 0;
    }
    return a.slug() < b.slug();
}

GameCatalog GameCatalog::loadFromDirectory(const std::filesystem::path& games_dir)
{
    GameCatalog catalog;

    std::error_code ec;
    if (!std::filesystem::is_directory(games_dir, ec)) {
        catalog.errors_.push_back({games_dir, "not a directory"});
        return catalog;
    }

    // Collected and sorted before loading, so the load order does not
    // depend on how the filesystem enumerates entries.
    std::vector<std::filesystem::path> game_dirs;
    for (const auto& entry : std::filesystem::directory_iterator(games_dir, ec)) {
        if (entry.is_directory(ec)) {
            game_dirs.push_back(entry.path());
        }
    }
    if (ec) {
        catalog.errors_.push_back({games_dir, "cannot read directory: " + ec.message()});
        return catalog;
    }
    std::sort(game_dirs.begin(), game_dirs.end());

    for (const auto& dir : game_dirs) {
        const auto slug = dir.filename().string();
        const auto toml = dir / (slug + ".toml");
        if (!std::filesystem::exists(toml, ec)) {
            // A directory without a definition is not an error: screenshot
            // and recipe material can land before the definition does.
            continue;
        }
        std::string error;
        auto game = GameDefinition::fromToml(toml, error);
        if (!game) {
            catalog.errors_.push_back({toml, error});
            continue;
        }
        if (catalog.find(game->slug()) != nullptr) {
            catalog.errors_.push_back({toml, "duplicate slug \"" + game->slug() + "\""});
            continue;
        }
        catalog.games_.push_back(std::move(*game));
    }

    std::sort(catalog.games_.begin(), catalog.games_.end(), title_precedes);
    return catalog;
}

const GameDefinition* GameCatalog::find(std::string_view slug) const
{
    const auto it = std::find_if(games_.begin(), games_.end(),
                                 [slug](const GameDefinition& game) {
                                     return game.slug() == slug;
                                 });
    return it == games_.end() ? nullptr : &*it;
}

}  // namespace showroom
