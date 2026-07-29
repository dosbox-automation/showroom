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
#include <string>

namespace showroom {

class GameLauncher;
class Sidebar;
class TileGrid;

// The window has no free size: it steps between the tile widths the
// screen can hold and remembers which step it was left at.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // The sizer is passed in so a test can state a screen; measuring the
    // primary screen here would make every window test depend on the
    // platform plugin's idea of a display. The launcher is borrowed, not
    // owned; without one the tiles stay static and touch no cache.
    MainWindow(const GameCatalog& catalog, const std::filesystem::path& assets_dir,
               Settings settings, StepSizer sizer, GameLauncher* launcher = nullptr,
               QWidget* parent = nullptr);

    static StepSizer sizerForPrimaryScreen();

    int tileWidth() const { return tile_width_px_; }
    const StepSizer& sizer() const { return sizer_; }
    TileGrid* grid() const { return grid_; }

public slots:
    void stepUp();
    void stepDown();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    // Virtual so window tests can answer without a desktop; the production
    // implementations show modal dialogs.
    virtual bool confirmGameSwitch(const QString& running_title,
                                   const QString& pending_title);
    virtual bool offerReinstall(const QString& title, const QString& detail);
    virtual bool confirmPortNotice(bool& dont_show_again);

private:
    void applyTileWidth(int width_px);
    void showAbout();
    void onTileAction(const QString& slug);
    void onGameEnded(const QString& slug);
    void launchGame(const GameDefinition& game);
    bool ensureIntactOrOffer(const GameDefinition& game);
    void demoteDamagedTile(const GameDefinition& game);
    bool confirmPortNoticeIfNeeded();

    // Fixed for the life of the window: dragged to a smaller monitor it
    // keeps its step rather than re-measuring behind the user's back.
    StepSizer sizer_;
    Settings settings_;
    GameCatalog catalog_;
    std::filesystem::path assets_dir_;

    GameLauncher* launcher_ = nullptr;
    Sidebar* sidebar_ = nullptr;
    TileGrid* grid_ = nullptr;
    int tile_width_px_ = 0;

    // Set when the user accepts a switch; the game launches once the
    // running one has actually exited, never beside it.
    std::string pending_switch_slug_;

    // resizeEvent applies a step, which resizes the window, which arrives
    // back here.
    bool applying_step_ = false;
};

} // namespace showroom

#endif // SHOWROOM_UI_MAIN_WINDOW_H
