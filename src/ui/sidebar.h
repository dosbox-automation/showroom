// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_SIDEBAR_H
#define SHOWROOM_UI_SIDEBAR_H

#include <QString>
#include <QWidget>

#include <filesystem>

namespace showroom {

// The narrow strip down the left: which engine version this showroom
// carries, the two project logos, and the three things the application
// itself can do.
class Sidebar : public QWidget {
    Q_OBJECT

public:
    // engine_version is the dosbox-automation version string, which is
    // the headline: the showroom exists to demonstrate that engine, so
    // its version is the one on display.
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
