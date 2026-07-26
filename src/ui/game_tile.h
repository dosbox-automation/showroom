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

// One game in the grid, 4:3. The tile owns no logic beyond what it draws:
// whether its action means download, install or launch is decided above it.
class GameTile : public QWidget {
    Q_OBJECT

public:
    // The game's own directory, holding its TOML and screenshots. A
    // missing screenshot leaves the image empty rather than failing the
    // tile - sixteen tiles must not depend on thirty-two files.
    GameTile(const GameDefinition& definition, const std::filesystem::path& assets_dir,
             QWidget* parent = nullptr);

    void setState(TileState state);
    void setProgress(int percent);

    TileState state() const { return state_; }
    const QString& slug() const { return slug_; }

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

    QPixmap scaled_;
    // The tile size it was built for, not the pixmap's own size: a
    // letterboxed capture never matches the tile and would rescale on
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
