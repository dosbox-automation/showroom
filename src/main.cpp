// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/paths.h"
#include "app/settings.h"
#include "app/logging.h"
#include "engine/game_launcher.h"
#include "net/downloader.h"
#include "model/game_catalog.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QIcon>

#include <memory>
#include <system_error>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("dosbox-automation-showroom"));
    QCoreApplication::setOrganizationName(QStringLiteral("dosbox-automation"));

    const std::filesystem::path assets = showroom::Paths::assetsDir();
    showroom::log_info("showroom", "assets: %s", assets.string().c_str());

    // The showroom wears the engine's icon: same product as far as a task
    // bar is concerned (Stefan, 2026-07-26). The desktop entry that makes
    // it stick belongs to packaging.
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

    // Named here because a game that silently went missing is the one bug
    // nobody would report.
    for (const showroom::CatalogLoadError& error : catalog.errors()) {
        showroom::log_error("showroom",
                            "%s: %s",
                            error.path.string().c_str(),
                            error.message.c_str());
    }

    // Without an engine the grid still shows; the tiles just have nothing
    // to offer, which beats refusing to start over a packaging problem.
    const std::filesystem::path engine_binary = showroom::Paths::engineBinary();
    std::unique_ptr<showroom::GameLauncher> launcher;
    std::error_code probe_error;
    if (std::filesystem::is_regular_file(engine_binary, probe_error)) {
        launcher = std::make_unique<showroom::GameLauncher>(engine_binary,
                                                            showroom::Paths::cacheDir());
        showroom::log_info("showroom", "engine: %s", engine_binary.string().c_str());
    } else {
        showroom::log_warn("showroom",
                           "no engine binary at %s, launching disabled",
                           engine_binary.string().c_str());
    }

    showroom::Downloader downloader;

    showroom::MainWindow window(catalog,
                                assets,
                                showroom::Settings(showroom::Paths::settingsFile()),
                                showroom::MainWindow::sizerForPrimaryScreen(),
                                launcher.get(),
                                &downloader);
    window.show();

    return QApplication::exec();
}
