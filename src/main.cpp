// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/paths.h"
#include "app/settings.h"
#include "app/logging.h"
#include "model/game_catalog.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("dosbox-automation-showroom"));
    QCoreApplication::setOrganizationName(QStringLiteral("dosbox-automation"));

    const std::filesystem::path assets = showroom::Paths::assetsDir();
    showroom::log_info("showroom", "assets: %s", assets.string().c_str());

    // The showroom wears the engine's icon: it is the same product as
    // far as anyone looking at a task bar is concerned (Stefan,
    // 2026-07-26). The desktop entry that makes this stick outside the
    // window itself belongs to packaging, in phase 8.
    const QIcon icon(QString::fromStdString(
            (assets / "logos" / "dosbox-automation.svg").string()));
    if (icon.isNull()) {
        showroom::log_warn("showroom",
                           "no application icon found in %s",
                           (assets / "logos").string().c_str());
    }
    QApplication::setWindowIcon(icon);

    const showroom::GameCatalog catalog = showroom::GameCatalog::loadFromDirectory(
            showroom::Paths::gamesDir());

    // A definition that failed to parse is left out of the grid rather
    // than blanking it, and every one of them is named here: a game that
    // silently went missing is the one bug nobody would report.
    for (const showroom::CatalogLoadError& error : catalog.errors()) {
        showroom::log_error("showroom",
                            "%s: %s",
                            error.path.string().c_str(),
                            error.message.c_str());
    }

    showroom::MainWindow window(catalog,
                                assets,
                                showroom::Settings(showroom::Paths::settingsFile()),
                                showroom::MainWindow::sizerForPrimaryScreen());
    window.show();

    return QApplication::exec();
}
