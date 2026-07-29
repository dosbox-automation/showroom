// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_SETTINGS_H
#define SHOWROOM_APP_SETTINGS_H

#include <QString>

#include <filesystem>

namespace showroom {

// In the cache because everything here is disposable, and nothing the
// showroom trusts for behaviour belongs in a writable file. The stored
// width is validated against the current screen before use.
class Settings {
public:
    explicit Settings(std::filesystem::path file);

    // Zero when nothing has been stored yet, which the caller reads as
    // "use the default step".
    int tileWidth() const;
    void setTileWidth(int width_px);

    // Defaults to true; only an explicit stored false suppresses it, so a
    // corrupt file fails toward showing the notice.
    bool showPortNotice() const;
    void setShowPortNotice(bool show);

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

} // namespace showroom

#endif // SHOWROOM_APP_SETTINGS_H
