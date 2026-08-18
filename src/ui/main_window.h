// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_MAIN_WINDOW_H
#define SHOWROOM_UI_MAIN_WINDOW_H

#include "app/settings.h"
#include "model/game_catalog.h"
#include "model/step_sizer.h"
#include "model/tile_state.h"
#include "net/download_plan.h"

#include <QMainWindow>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class QTimer;

namespace showroom {

class Connectivity;
class Downloader;
class GameLauncher;
class InstallRunner;
class Sidebar;
class TileGrid;

// The window has no free size: it steps between the tile widths the
// screen can hold and remembers which step it was left at.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // The sizer is passed in so a test can state a screen; measuring the
    // primary screen here would make every window test depend on the
    // platform plugin's idea of a display. Launcher and downloader are
    // borrowed, not owned; without them the matching tile actions are
    // disabled and no cache is touched.
    MainWindow(const GameCatalog& catalog, const std::filesystem::path& assets_dir,
               Settings settings, StepSizer sizer, GameLauncher* launcher = nullptr,
               Downloader* downloader = nullptr, Connectivity* connectivity = nullptr,
               InstallRunner* install_runner = nullptr, QWidget* parent = nullptr);

    static StepSizer sizerForPrimaryScreen();

    // How long a resize must stay quiet before the window snaps back to
    // its step's exact size. Public so tests can wait past it.
    static constexpr int kResizeSettleMs = 200;

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
    void snapToStepGeometry();
    void showAbout();
    void onTileAction(const QString& slug);
    void onGameEnded(const QString& slug);
    void launchGame(const GameDefinition& game);
    bool ensureIntactOrOffer(const GameDefinition& game);
    void demoteDamagedTile(const GameDefinition& game);
    bool confirmPortNoticeIfNeeded();
    void startDownload(const GameDefinition& game);
    bool startPlannedDownload();
    void onDownloadFailed(const QString& reason);
    void startInstall(const GameDefinition& game);
    void setDownloadingTileState(TileState state);

    // Every tile but the installing one, which keeps its progress live.
    void lockTilesForInstall(const QString& installing_slug);
    void unlockTiles();

    // Qt stacks override cursors, so the push is guarded: a second push
    // or a missed pop would leave the busy shape on for the whole run.
    void pushBusyCursor();
    void popBusyCursor();

    bool busy_cursor_pushed_ = false;
    void applyOnlineState(bool online);

    // Fixed for the life of the window: dragged to a smaller monitor it
    // keeps its step rather than re-measuring behind the user's back.
    StepSizer sizer_;
    Settings settings_;
    GameCatalog catalog_;
    std::filesystem::path assets_dir_;

    GameLauncher* launcher_ = nullptr;
    Downloader* downloader_ = nullptr;
    Connectivity* connectivity_ = nullptr;
    InstallRunner* install_runner_ = nullptr;
    Sidebar* sidebar_ = nullptr;
    TileGrid* grid_ = nullptr;
    int tile_width_px_ = 0;

    // Set when the user accepts a switch; the game launches once the
    // running one has actually exited, never beside it.
    std::string pending_switch_slug_;

    // Same move for an install accepted while a game runs: the engine
    // port is shared, so the install starts only after the exit.
    std::string pending_install_slug_;

    // One transfer at a time; the downloader's signals carry no slug, so
    // the window remembers whose tile they belong to.
    std::string downloading_slug_;

    // The source chain for the running transfer: a failure advances to
    // the next plan before the tile gives up (aug-ctpt).
    std::vector<DownloadPlan> downloading_plans_;
    std::size_t downloading_plan_index_ = 0;

    // resizeEvent applies a step, which resizes the window, which arrives
    // back here.
    bool applying_step_ = false;

    // A resize inside a drag cannot be fought; this fires once the drag
    // has settled and snaps the window onto its step's exact size.
    QTimer* resize_settle_timer_ = nullptr;
};

} // namespace showroom

#endif // SHOWROOM_UI_MAIN_WINDOW_H
