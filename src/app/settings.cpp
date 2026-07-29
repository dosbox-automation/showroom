// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/settings.h"

#include <QSettings>
#include <QVariant>

#include <utility>

namespace showroom {
namespace {

constexpr const char* kTileWidthKey = "window/tile_width_px";
constexpr const char* kShowPortNoticeKey = "launch/show_port_notice";

// A stored width larger than any screen would still be filtered by the
// sizer, but a wildly out of range number should never get that far.
constexpr int kSanityCeilingPx = 4096;

} // namespace

Settings::Settings(std::filesystem::path file) : file_(std::move(file)) {}

int Settings::tileWidth() const
{
    QSettings settings(QString::fromStdString(file_.string()), QSettings::IniFormat);

    bool parsed = false;
    const int width = settings.value(QString::fromLatin1(kTileWidthKey)).toInt(&parsed);
    if (!parsed || width <= 0 || width > kSanityCeilingPx) {
        return 0;
    }
    return width;
}

bool Settings::showPortNotice() const
{
    QSettings settings(QString::fromStdString(file_.string()), QSettings::IniFormat);

    // String-compared rather than toBool(): toBool() maps garbage to
    // false, which would suppress the notice on a corrupt file.
    return settings.value(QString::fromLatin1(kShowPortNoticeKey)).toString()
        != QLatin1String("false");
}

void Settings::setShowPortNotice(bool show)
{
    QSettings settings(QString::fromStdString(file_.string()), QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kShowPortNoticeKey), show);
}

void Settings::setTileWidth(int width_px)
{
    if (width_px <= 0 || width_px > kSanityCeilingPx) {
        return;
    }

    QSettings settings(QString::fromStdString(file_.string()), QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kTileWidthKey), width_px);
}

} // namespace showroom
