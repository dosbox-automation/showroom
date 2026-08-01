// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/main_window.h"

#include "app/logging.h"
#include "app/paths.h"
#include "engine/game_launcher.h"
#include "model/install_check.h"
#include "model/tile_state.h"
#include "net/download_plan.h"
#include "net/connectivity.h"
#include "net/downloader.h"
#include "ui/about_dialog.h"
#include "ui/game_tile.h"
#include "ui/sidebar.h"
#include "ui/theme.h"
#include "ui/tile_grid.h"
#include "ui/version.h"

#include <QApplication>
#include <QCheckBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>

#include <algorithm>
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
                       StepSizer sizer, GameLauncher* launcher, Downloader* downloader,
                       Connectivity* connectivity, QWidget* parent)
        : QMainWindow(parent),
          sizer_(std::move(sizer)),
          settings_(std::move(settings)),
          catalog_(catalog),
          assets_dir_(assets_dir),
          launcher_(launcher),
          downloader_(downloader),
          connectivity_(connectivity)
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

        connect(launcher_, &GameLauncher::gameStarted, this, [this](const QString& slug) {
            if (GameTile* tile = grid_->tileFor(slug)) {
                tile->setState(TileState::Running);
            }
        });
        connect(launcher_, &GameLauncher::gameEnded, this, &MainWindow::onGameEnded);
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

    connect(grid_, &TileGrid::actionTriggered, this, &MainWindow::onTileAction);

    if (downloader_ != nullptr) {
        connect(downloader_,
                &Downloader::progress,
                this,
                [this](qint64 received, qint64 total) {
                    if (total <= 0) {
                        return;
                    }
                    if (GameTile* tile = grid_->tileFor(
                                QString::fromStdString(downloading_slug_))) {
                        tile->setProgress(static_cast<int>(
                                std::clamp<qint64>(received * 100 / total, 0, 100)));
                    }
                });
        connect(downloader_, &Downloader::finished, this, [this](const QString&) {
            setDownloadingTileState(TileState::Downloaded);
        });
        connect(downloader_, &Downloader::failed, this, [this](const QString& reason) {
            log_error(kLogComponent,
                      "download of %s failed: %s",
                      downloading_slug_.c_str(),
                      reason.toStdString().c_str());
            setDownloadingTileState(TileState::NotDownloaded);
        });
        connect(downloader_, &Downloader::cancelled, this, [this]() {
            setDownloadingTileState(TileState::NotDownloaded);
        });
    }

    if (connectivity_ != nullptr) {
        applyOnlineState(connectivity_->isOnline());
        connect(connectivity_, &Connectivity::onlineChanged,
                this, &MainWindow::applyOnlineState);
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Bounded by the launcher's stop escalation: a stuck emulator ends
    // killed rather than keeping the showroom alive.
    if (launcher_ != nullptr && launcher_->isRunning()) {
        launcher_->shutdownAndWait();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::onTileAction(const QString& slug)
{
    GameTile* tile = grid_->tileFor(slug);
    const GameDefinition* game = catalog_.find(slug.toStdString());
    if (tile == nullptr || game == nullptr) {
        return;
    }
    switch (actionFor(tile->state())) {
    case TileAction::Play: {
        if (launcher_ == nullptr) {
            break;
        }
        if (!ensureIntactOrOffer(*game)) {
            break;
        }
        if (!confirmPortNoticeIfNeeded()) {
            break;
        }
        if (launcher_->isRunning()) {
            const GameDefinition* running = catalog_.find(
                    launcher_->runningSlug().toStdString());
            const QString running_title = running != nullptr
                                                ? QString::fromStdString(running->title())
                                                : launcher_->runningSlug();
            if (!confirmGameSwitch(running_title,
                                   QString::fromStdString(game->title()))) {
                break;
            }
            pending_switch_slug_ = game->slug();
            launcher_->stop();
            break;
        }
        launchGame(*game);
        break;
    }
    case TileAction::Stop:
        if (launcher_ != nullptr) {
            launcher_->stop();
        }
        break;
    case TileAction::Download: startDownload(*game); break;
    case TileAction::Cancel:
        if (downloader_ != nullptr) {
            downloader_->cancel();
        }
        break;
    default: break;
    }
}

void MainWindow::applyOnlineState(bool online)
{
    for (const auto& game : catalog_) {
        GameTile* tile = grid_->tileFor(QString::fromStdString(game.slug()));
        if (tile == nullptr) {
            continue;
        }
        if (online && tile->state() == TileState::OfflineNotDownloaded) {
            tile->setState(TileState::NotDownloaded);
        } else if (!online && tile->state() == TileState::NotDownloaded) {
            tile->setState(TileState::OfflineNotDownloaded);
        }
    }
}

void MainWindow::startDownload(const GameDefinition& game)
{
    if (downloader_ == nullptr || downloader_->isRunning()) {
        return;
    }
    if (connectivity_ != nullptr && !connectivity_->isOnline()) {
        log_warn(kLogComponent, "download of %s blocked: offline", game.slug().c_str());
        return;
    }
    const auto plan = downloadPlanFor(game);
    const auto download_dir = Paths::downloadDirFor(game.slug());
    if (!plan || !download_dir) {
        log_error(kLogComponent, "%s has no usable download source", game.slug().c_str());
        return;
    }

    std::string error;
    if (!downloader_->start(QUrl(QString::fromStdString(plan->url)),
                            *download_dir / plan->filename,
                            error,
                            plan->size)) {
        log_error(kLogComponent,
                  "cannot download %s: %s",
                  game.slug().c_str(),
                  error.c_str());
        return;
    }

    downloading_slug_ = game.slug();
    if (GameTile* tile = grid_->tileFor(QString::fromStdString(game.slug()))) {
        tile->setState(TileState::Downloading);
        tile->setProgress(0);
    }
}

void MainWindow::setDownloadingTileState(TileState state)
{
    if (state == TileState::NotDownloaded
        && connectivity_ != nullptr && !connectivity_->isOnline()) {
        state = TileState::OfflineNotDownloaded;
    }
    if (GameTile* tile = grid_->tileFor(QString::fromStdString(downloading_slug_))) {
        tile->setState(state);
    }
    downloading_slug_.clear();
}

void MainWindow::onGameEnded(const QString& slug)
{
    if (GameTile* tile = grid_->tileFor(slug)) {
        tile->setState(TileState::Ready);
    }
    if (pending_switch_slug_.empty()) {
        return;
    }
    const GameDefinition* pending = catalog_.find(pending_switch_slug_);
    pending_switch_slug_.clear();
    // Re-checked: the files can change between accepting the switch and
    // the running game's exit.
    if (pending != nullptr && ensureIntactOrOffer(*pending)) {
        launchGame(*pending);
    }
}

bool MainWindow::confirmPortNoticeIfNeeded()
{
    if (!settings_.showPortNotice()) {
        return true;
    }
    bool dont_show_again = false;
    if (!confirmPortNotice(dont_show_again)) {
        return false;
    }
    // Only an accepted notice may suppress: honoring the checkbox on a
    // decline would silently disable launching for good.
    if (dont_show_again) {
        settings_.setShowPortNotice(false);
    }
    return true;
}

bool MainWindow::ensureIntactOrOffer(const GameDefinition& game)
{
    const auto install_dir = Paths::installDirFor(game.slug());
    if (!install_dir) {
        return false;
    }
    const auto damage = installDamage(game, *install_dir);
    if (damage.empty()) {
        return true;
    }
    log_warn(kLogComponent,
             "%s failed the pre-launch check, %d problems, first: %s",
             game.slug().c_str(),
             static_cast<int>(damage.size()),
             damage.front().c_str());
    if (offerReinstall(QString::fromStdString(game.title()),
                       QString::fromStdString(damage.front()))) {
        demoteDamagedTile(game);
    }
    return false;
}

void MainWindow::demoteDamagedTile(const GameDefinition& game)
{
    GameTile* tile = grid_->tileFor(QString::fromStdString(game.slug()));
    if (tile == nullptr) {
        return;
    }
    // Reinstallation rides the normal download/install road, so the tile
    // goes back to the station whose artifact is still on disk.
    std::error_code ec;
    const auto downloads = Paths::downloadDirFor(game.slug());
    const bool archive_present = downloads.has_value()
                              && std::filesystem::is_directory(*downloads, ec)
                              && std::filesystem::directory_iterator(*downloads, ec)
                                         != std::filesystem::directory_iterator();
    tile->setState(archive_present ? TileState::Downloaded : TileState::NotDownloaded);
}

void MainWindow::launchGame(const GameDefinition& game)
{
    std::string error;
    if (!launcher_->launch(game, error)) {
        log_error(kLogComponent,
                  "cannot launch %s: %s",
                  game.slug().c_str(),
                  error.c_str());
    }
}

bool MainWindow::confirmPortNotice(bool& dont_show_again)
{
    QMessageBox box(QMessageBox::Information,
                    QStringLiteral("Before the game starts"),
                    QStringLiteral("The showroom controls the game through its "
                                   "bundled dosbox-automation engine over "
                                   "localhost port %1.\n\nIf a firewall asks "
                                   "about this connection, allow it - without "
                                   "it the game cannot start or be "
                                   "controlled.\n\nStart the game?")
                            .arg(kShowroomEnginePort),
                    QMessageBox::Yes | QMessageBox::No,
                    this);
    box.setDefaultButton(QMessageBox::Yes);
    auto* checkbox = new QCheckBox(QStringLiteral("Do not show this again"), &box);
    box.setCheckBox(checkbox);

    const bool accepted = box.exec() == QMessageBox::Yes;
    dont_show_again = checkbox->isChecked();
    return accepted;
}

bool MainWindow::offerReinstall(const QString& title, const QString& detail)
{
    return QMessageBox::question(this,
                                 QStringLiteral("Damaged install"),
                                 QStringLiteral("%1's files are missing or damaged (%2). "
                                                "Reinstall %1?")
                                         .arg(title, detail),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No)
        == QMessageBox::Yes;
}

bool MainWindow::confirmGameSwitch(const QString& running_title,
                                   const QString& pending_title)
{
    return QMessageBox::question(this,
                                 QStringLiteral("Game running"),
                                 QStringLiteral(
                                         "%1 is still running. Stop it and start %2?")
                                         .arg(running_title, pending_title),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No)
        == QMessageBox::Yes;
}

void MainWindow::showAbout()
{
    AboutDialog dialog(catalog_, assets_dir_, this);
    dialog.exec();
}

} // namespace showroom
