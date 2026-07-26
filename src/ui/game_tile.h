// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_GAME_TILE_H
#define SHOWROOM_UI_GAME_TILE_H

#include "model/game_definition.h"
#include "model/tile_state.h"

#include <QPixmap>
#include <QString>
#include <QWidget>

#include <filesystem>

namespace showroom {

class LegendBar;

// One game in the grid: a 4:3 screenshot with the legend bar over its
// lower edge.
//
// The tile owns no logic beyond what it draws. It is told which state to
// show and reports that its action was triggered; deciding whether that
// means download, install or launch belongs above it.
class GameTile : public QWidget {
    Q_OBJECT

public:
    // assets_dir is the game's own directory, the one holding its TOML
    // and its two screenshots. A missing or unreadable screenshot leaves
    // the image area empty rather than failing the tile: sixteen tiles
    // must not depend on thirty-two files all being present.
    GameTile(const GameDefinition& definition, const std::filesystem::path& assets_dir,
             QWidget* parent = nullptr);

    void setState(TileState state);
    void setProgress(int percent);

    TileState state() const { return state_; }
    const QString& slug() const { return slug_; }

    // 4:3, image and legend together.
    void setTileWidth(int width_px);

signals:
    void actionTriggered(const QString& slug);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    const QPixmap& currentPixmap() const;
    void updateScaledPixmap();
    QString overlayMessage() const;

    QString slug_;
    QPixmap title_shot_;
    QPixmap gameplay_shot_;

    // Scaled to the current tile size, and desaturated when the state
    // calls for it. Rebuilt on resize and on any change that alters
    // which of the two shots is shown.
    QPixmap scaled_;
    // The tile size the cached pixmap was built for, not the pixmap's
    // own size: a letterboxed capture is smaller than the tile in one
    // direction, so its own size never matches and would rescale on
    // every repaint.
    QSize scaled_for_size_;
    bool scaled_is_grayscale_ = false;
    bool scaled_is_gameplay_ = false;

    TileState state_ = TileState::NotDownloaded;
    int progress_percent_ = 0;
    bool hovered_ = false;

    LegendBar* legend_ = nullptr;
};

} // namespace showroom

#endif // SHOWROOM_UI_GAME_TILE_H
