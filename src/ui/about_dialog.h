// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_ABOUT_DIALOG_H
#define SHOWROOM_UI_ABOUT_DIALOG_H

#include "model/game_catalog.h"

#include <QDialog>

#include <filesystem>

namespace showroom {

// The game strip is an inventory rather than a second launcher: the
// thumbnails ignore the pointer.
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    AboutDialog(const GameCatalog& catalog, const std::filesystem::path& assets_dir,
                QWidget* parent = nullptr);
};

} // namespace showroom

#endif // SHOWROOM_UI_ABOUT_DIALOG_H
