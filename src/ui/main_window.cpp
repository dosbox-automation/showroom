// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/main_window.h"

#include "app/logging.h"
#include "app/paths.h"
#include "engine/game_launcher.h"
#include "model/tile_state.h"
#include "ui/about_dialog.h"
#include "ui/game_tile.h"
#include "ui/sidebar.h"
#include "ui/theme.h"
#include "ui/tile_grid.h"
#include "ui/version.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QShortcut>

#include <utility>

namespace showroom {
namespace {

constexpr const char* kLogComponent = "main_window";

} // namespace

StepSizer MainWindow::sizerForPrimaryScreen()
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        log_warn(kLogComponent, "no screen reported, falling back to the smallest step");
        return StepSizer(GridChrome{}, 0, 0);
    }

    const QSize usable = screen->availableGeometry().size();
    return StepSizer(GridChrome{}, usable.width(), usable.height());
}

MainWindow::MainWindow(const GameCatalog& catalog,
                       const std::filesystem::path& assets_dir, Settings settings,
                       StepSizer sizer, GameLauncher* launcher, QWidget* parent)
        : QMainWindow(parent),
          sizer_(std::move(sizer)),
          settings_(std::move(settings)),
          catalog_(catalog),
          assets_dir_(assets_dir),
          launcher_(launcher)
{
    setWindowTitle(QStringLiteral("dosbox-automation showroom"));
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, theme::kWindowBackground);
    background.setColor(QPalette::WindowText, theme::kPrimaryText);
    setPalette(background);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new Sidebar(QString::fromLatin1(kBundledEngineVersion),
                           assets_dir_ / "logos",
                           central);
    connect(sidebar_, &Sidebar::aboutRequested, this, &MainWindow::showAbout);
    connect(sidebar_, &Sidebar::quitRequested, this, &MainWindow::close);
    layout->addWidget(sidebar_);

    grid_ = new TileGrid(catalog_, assets_dir_ / "games", sizer_.chrome(), central);
    layout->addWidget(grid_, 1);

    if (launcher_ != nullptr) {
        std::error_code probe_error;
        for (std::size_t i = 0; i < catalog_.size(); ++i) {
            const GameDefinition& game = catalog_.at(i);
            // A game that cannot launch keeps its NoRecipe tile even if an
            // install directory is lying around.
            if (!game.isLaunchable() || !isSafeSlug(game.slug())) {
                continue;
            }
            if (std::filesystem::is_directory(Paths::installsDir() / game.slug(),
                                              probe_error)) {
                if (GameTile* tile = grid_->tileFor(
                            QString::fromStdString(game.slug()))) {
                    tile->setState(TileState::Ready);
                }
            }
        }

        connect(grid_, &TileGrid::actionTriggered, this, &MainWindow::onTileAction);
        connect(launcher_, &GameLauncher::gameStarted, this, [this](const QString& slug) {
            if (GameTile* tile = grid_->tileFor(slug)) {
                tile->setState(TileState::Running);
            }
        });
        connect(launcher_, &GameLauncher::gameEnded, this, [this](const QString& slug) {
            if (GameTile* tile = grid_->tileFor(slug)) {
                tile->setState(TileState::Ready);
            }
        });
        connect(launcher_,
                &GameLauncher::launchFailed,
                this,
                [this](const QString& slug, const QString& reason) {
                    log_error(kLogComponent,
                              "launch of %s failed: %s",
                              slug.toStdString().c_str(),
                              reason.toStdString().c_str());
                    if (GameTile* tile = grid_->tileFor(slug)) {
                        tile->setState(TileState::Ready);
                    }
                });
    }

    setCentralWidget(central);

    auto* zoom_in = new QShortcut(QKeySequence::ZoomIn, this);
    connect(zoom_in, &QShortcut::activated, this, &MainWindow::stepUp);
    // ZoomIn is Ctrl+Plus, which needs a shift on most layouts. Ctrl+=
    // is the key people actually press for it.
    auto* zoom_in_plain = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    connect(zoom_in_plain, &QShortcut::activated, this, &MainWindow::stepUp);

    auto* zoom_out = new QShortcut(QKeySequence::ZoomOut, this);
    connect(zoom_out, &QShortcut::activated, this, &MainWindow::stepDown);

    // The window steps rather than resizing freely: the increment is one
    // tile step across all four columns and rows, so a window manager
    // that honours size increments snaps the drag for us.
    const WindowSize smallest = sizer_.windowSizeFor(
            sizer_.tileWidths().empty() ? kMinTileWidthPx : sizer_.tileWidths().front());
    const WindowSize largest = sizer_.windowSizeFor(
            sizer_.tileWidths().empty() ? kMinTileWidthPx : sizer_.tileWidths().back());
    setMinimumSize(smallest.width_px, smallest.height_px);
    setMaximumSize(largest.width_px, largest.height_px);
    setBaseSize(smallest.width_px, smallest.height_px);
    setSizeIncrement(sizer_.chrome().columns * kTileStepPx,
                     sizer_.chrome().rows * StepSizer::tileHeightFor(kTileStepPx));

    const int stored = settings_.tileWidth();
    // A width remembered on another monitor may not be offered here, so
    // it is snapped rather than trusted.
    const int initial = stored > 0 ? sizer_.snapToStep(stored)
                                   : sizer_.defaultTileWidth();
    applyTileWidth(initial);

    log_info(kLogComponent,
             "window opened at tile width %d with %d games",
             tile_width_px_,
             static_cast<int>(catalog_.size()));
}

void MainWindow::applyTileWidth(int width_px)
{
    if (width_px <= 0) {
        return;
    }

    tile_width_px_ = width_px;
    grid_->setTileWidth(width_px);

    const WindowSize target = sizer_.windowSizeFor(width_px);
    applying_step_ = true;
    resize(target.width_px, target.height_px);
    applying_step_ = false;

    settings_.setTileWidth(width_px);
}

void MainWindow::stepUp()
{
    const int next = sizer_.nextTileWidth(tile_width_px_);
    if (next != tile_width_px_) {
        applyTileWidth(next);
    }
}

void MainWindow::stepDown()
{
    const int previous = sizer_.previousTileWidth(tile_width_px_);
    if (previous != tile_width_px_) {
        applyTileWidth(previous);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    if (applying_step_) {
        return;
    }

    // A window manager that ignores size increments hands us an
    // arbitrary size; take the largest step that fits inside it so the
    // grid still lands on a whole number of tiles.
    const int width = sizer_.tileWidthForWindowSize(size().width(), size().height());
    if (width != tile_width_px_) {
        applyTileWidth(width);
    }
}

void MainWindow::onTileAction(const QString& slug)
{
    if (launcher_ == nullptr) {
        return;
    }
    GameTile* tile = grid_->tileFor(slug);
    const GameDefinition* game = catalog_.find(slug.toStdString());
    if (tile == nullptr || game == nullptr) {
        return;
    }
    switch (actionFor(tile->state())) {
    case TileAction::Play: {
        std::string error;
        if (!launcher_->launch(*game, error)) {
            log_error(kLogComponent,
                      "cannot launch %s: %s",
                      game->slug().c_str(),
                      error.c_str());
        }
        break;
    }
    case TileAction::Stop: launcher_->stop(); break;
    default: break;
    }
}

void MainWindow::showAbout()
{
    AboutDialog dialog(catalog_, assets_dir_, this);
    dialog.exec();
}

} // namespace showroom
