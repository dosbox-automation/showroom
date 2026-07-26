// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_MAIN_WINDOW_H
#define SHOWROOM_UI_MAIN_WINDOW_H

#include "app/settings.h"
#include "model/game_catalog.h"
#include "model/step_sizer.h"

#include <QMainWindow>

#include <filesystem>

namespace showroom {

class Sidebar;
class TileGrid;

// The whole application window: sidebar on the left, tile grid filling
// the rest.
//
// The window has no free size. It steps between the tile widths the
// current screen can hold, by keyboard or by dragging an edge, and
// remembers which step it was left at.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // The sizer is passed in rather than built here, so which steps
    // exist is a decision the caller makes and a test can state. Reading
    // it off the primary screen inside the constructor would make every
    // window test depend on the platform plugin's idea of a display.
    MainWindow(const GameCatalog& catalog, const std::filesystem::path& assets_dir,
               Settings settings, StepSizer sizer, QWidget* parent = nullptr);

    // The steps the screen this process opened on can hold.
    static StepSizer sizerForPrimaryScreen();

    int tileWidth() const { return tile_width_px_; }
    const StepSizer& sizer() const { return sizer_; }

public slots:
    void stepUp();
    void stepDown();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTileWidth(int width_px);
    void showAbout();

    // Fixed for the life of the window. A window dragged to a smaller
    // monitor keeps its step; nothing here re-measures behind the
    // user's back.
    StepSizer sizer_;
    Settings settings_;
    GameCatalog catalog_;
    std::filesystem::path assets_dir_;

    Sidebar* sidebar_ = nullptr;
    TileGrid* grid_ = nullptr;
    int tile_width_px_ = 0;

    // resizeEvent applies a step, which resizes the window, which
    // arrives back here. One flag is cheaper than an event filter.
    bool applying_step_ = false;
};

} // namespace showroom

#endif // SHOWROOM_UI_MAIN_WINDOW_H
