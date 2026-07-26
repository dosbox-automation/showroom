// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_PATHS_H
#define SHOWROOM_APP_PATHS_H

#include <filesystem>
#include <optional>
#include <string_view>

namespace showroom {

// Lexical only; backs up the slug validators rather than replacing them.
bool isWithin(const std::filesystem::path& base, const std::filesystem::path& candidate);

// Assets are read-only and ship with the binary; the cache is writable
// and trusted for nothing.
class Paths {
public:
    // SHOWROOM_ASSETS_DIR, then beside the executable, then the source
    // tree. The override grants no more than replacing the binary does.
    static std::filesystem::path assetsDir();

    static std::filesystem::path gamesDir() { return assetsDir() / "games"; }
    static std::filesystem::path logosDir() { return assetsDir() / "logos"; }

    // Created private to the user, beside the executable on Windows to
    // stay portable. A broken SHOWROOM_CACHE_DIR is reported, never
    // replaced by the real cache.
    static std::filesystem::path cacheDir();

    static std::filesystem::path downloadsDir();
    static std::filesystem::path installsDir();

    // Empty for an unsafe slug, rather than a path that looks usable.
    static std::optional<std::filesystem::path> downloadDirFor(std::string_view slug);
    static std::optional<std::filesystem::path> installDirFor(std::string_view slug);

    // The engine treats the last -conf file's directory as an allowed
    // mount root, so moving this out of the cache stops installs being
    // mountable.
    static std::filesystem::path runConfFile() { return cacheDir() / "run.conf"; }

    static std::filesystem::path settingsFile() { return cacheDir() / "showroom.ini"; }
};

} // namespace showroom

#endif // SHOWROOM_APP_PATHS_H
