// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_SIDEBAR_H
#define SHOWROOM_UI_SIDEBAR_H

#include <QString>
#include <QWidget>

#include <filesystem>

namespace showroom {

class Sidebar : public QWidget {
    Q_OBJECT

public:
    // The engine version, not the showroom's: that is the one on display.
    Sidebar(const QString& engine_version, const std::filesystem::path& logos_dir,
            QWidget* parent = nullptr);

signals:
    void aboutRequested();
    void updateRequested();
    void quitRequested();

private:
    void openUrl(const QString& url) const;
};

} // namespace showroom

#endif // SHOWROOM_UI_SIDEBAR_H
