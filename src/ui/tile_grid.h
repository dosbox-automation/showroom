// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_TILE_GRID_H
#define SHOWROOM_UI_TILE_GRID_H

#include "model/game_catalog.h"
#include "model/step_sizer.h"
#include "model/tile_state.h"

#include <QString>
#include <QVector>
#include <QWidget>

#include <filesystem>

namespace showroom {

class GameTile;

// Built once and never rebuilt: bundled games cannot appear or disappear
// while the showroom runs, only change state.
class TileGrid : public QWidget {
    Q_OBJECT

public:
    // Fewer games than the grid holds leaves the remaining cells empty
    // rather than reflowing.
    TileGrid(const GameCatalog& catalog, const std::filesystem::path& games_dir,
             const GridChrome& chrome, QWidget* parent = nullptr);

    void setTileWidth(int width_px);

    GameTile* tileFor(const QString& slug) const;

    int tileCount() const { return static_cast<int>(tiles_.size()); }

signals:
    void actionTriggered(const QString& slug);

private:
    GridChrome chrome_;
    QVector<GameTile*> tiles_;
};

} // namespace showroom

#endif // SHOWROOM_UI_TILE_GRID_H
