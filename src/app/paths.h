// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_PATHS_H
#define SHOWROOM_APP_PATHS_H

#include <filesystem>

namespace showroom {

// Where the bundled assets and the writable cache live.
//
// These are two different kinds of place and the difference is a
// security boundary, not a convenience: the assets are read-only and
// trusted because they ship with the binary, and the cache is writable
// and trusted for nothing. Game definitions are only ever read from the
// first.
class Paths {
public:
    // Resolution order for the assets, first hit wins:
    //   1. SHOWROOM_ASSETS_DIR, for tests and for a packaged tree that
    //      puts them somewhere else. It changes only what this process
    //      reads for its own user, which is no more than replacing the
    //      binary already allows.
    //   2. <directory of the executable>/assets, which is the AppImage
    //      and the Windows zip.
    //   3. The source tree it was built from, so a developer build runs
    //      without installing anything.
    static std::filesystem::path assetsDir();

    static std::filesystem::path gamesDir() { return assetsDir() / "games"; }
    static std::filesystem::path logosDir() { return assetsDir() / "logos"; }

    // Downloads, installs and the generated run conf. Created on first
    // use. On Windows this sits next to the executable so the whole
    // thing stays portable; elsewhere it is the XDG cache directory.
    static std::filesystem::path cacheDir();

    static std::filesystem::path settingsFile() { return cacheDir() / "showroom.ini"; }
};

} // namespace showroom

#endif // SHOWROOM_APP_PATHS_H
