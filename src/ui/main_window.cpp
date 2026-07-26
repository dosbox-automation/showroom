// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/main_window.h"

#include "app/logging.h"
#include "ui/about_dialog.h"
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
                       StepSizer sizer, QWidget* parent)
        : QMainWindow(parent),
          sizer_(std::move(sizer)),
          settings_(std::move(settings)),
          catalog_(catalog),
          assets_dir_(assets_dir)
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

void MainWindow::showAbout()
{
    AboutDialog dialog(catalog_, assets_dir_, this);
    dialog.exec();
}

} // namespace showroom
