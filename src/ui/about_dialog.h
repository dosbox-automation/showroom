// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_ABOUT_DIALOG_H
#define SHOWROOM_UI_ABOUT_DIALOG_H

#include "model/game_catalog.h"

#include <QDialog>

#include <filesystem>

namespace showroom {

// What this is, who made the pieces, and every game it carries.
//
// The game strip is a visual inventory rather than a second launcher:
// the thumbnails do not respond to the pointer and clicking one does
// nothing.
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    AboutDialog(const GameCatalog& catalog, const std::filesystem::path& assets_dir,
                QWidget* parent = nullptr);
};

} // namespace showroom

#endif // SHOWROOM_UI_ABOUT_DIALOG_H
