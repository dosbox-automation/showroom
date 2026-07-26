// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_SETTINGS_H
#define SHOWROOM_APP_SETTINGS_H

#include <QString>

#include <filesystem>

namespace showroom {

// The little the showroom remembers between runs.
//
// Deliberately tiny and deliberately in the cache: everything here can
// be thrown away without losing anything the user would miss, and a
// file the showroom trusts for behaviour has no business being
// writable. The stored tile width is validated against what the current
// screen can actually hold before it is used.
class Settings {
public:
    explicit Settings(std::filesystem::path file);

    // Zero when nothing has been stored yet, which the caller reads as
    // "use the default step".
    int tileWidth() const;
    void setTileWidth(int width_px);

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

} // namespace showroom

#endif // SHOWROOM_APP_SETTINGS_H
