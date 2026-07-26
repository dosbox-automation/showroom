// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_GAME_CATALOG_H
#define SHOWROOM_MODEL_GAME_CATALOG_H

#include "model/game_definition.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace showroom {

// Collected rather than only logged: a failure that reached stderr alone
// would be invisible in the packaged app.
struct CatalogLoadError {
    std::filesystem::path path;
    std::string message;
};

// Every bundled game definition, in display order. One unreadable file
// must never blank the grid, so a bad definition is skipped and recorded
// in errors().
class GameCatalog {
public:
    // Scans games_dir for <slug>/<slug>.toml. A missing directory yields
    // an empty catalogue with one error, not a throw.
    static GameCatalog loadFromDirectory(const std::filesystem::path& games_dir);

    std::size_t size() const { return games_.size(); }
    bool empty() const { return games_.empty(); }
    const GameDefinition& at(std::size_t index) const { return games_.at(index); }

    const GameDefinition* find(std::string_view slug) const;

    const std::vector<CatalogLoadError>& errors() const { return errors_; }

    std::vector<GameDefinition>::const_iterator begin() const { return games_.begin(); }
    std::vector<GameDefinition>::const_iterator end() const { return games_.end(); }

private:
    std::vector<GameDefinition> games_;
    std::vector<CatalogLoadError> errors_;
};

// By title, ignoring case; ties fall back to the unique slug so the order
// never depends on how the filesystem enumerates.
bool titlePrecedes(const GameDefinition& a, const GameDefinition& b);

} // namespace showroom

#endif // SHOWROOM_MODEL_GAME_CATALOG_H
