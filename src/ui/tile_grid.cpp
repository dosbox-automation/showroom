// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/tile_grid.h"

#include "ui/game_tile.h"

#include <QGridLayout>

namespace showroom {

TileGrid::TileGrid(const GameCatalog& catalog, const std::filesystem::path& games_dir,
                   const GridChrome& chrome, QWidget* parent)
        : QWidget(parent),
          chrome_(chrome)
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(chrome_.padding_px,
                               chrome_.padding_px,
                               chrome_.padding_px,
                               chrome_.padding_px);
    layout->setSpacing(chrome_.gap_px);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    const int columns = chrome_.columns > 0 ? chrome_.columns : 1;
    int index = 0;
    for (const GameDefinition& definition : catalog) {
        auto* tile = new GameTile(definition, games_dir / definition.slug(), this);
        connect(tile, &GameTile::actionTriggered, this, &TileGrid::actionTriggered);

        layout->addWidget(tile, index / columns, index % columns);
        tiles_.push_back(tile);
        ++index;
    }
}

void TileGrid::setTileWidth(int width_px)
{
    for (GameTile* tile : tiles_) {
        tile->setTileWidth(width_px);
    }
    // The layout is fixed to its contents, so it has to be told the
    // tiles changed size before it recomputes the grid.
    if (layout() != nullptr) {
        layout()->activate();
    }
    adjustSize();
}

GameTile* TileGrid::tileFor(const QString& slug) const
{
    for (GameTile* tile : tiles_) {
        if (tile->slug() == slug) {
            return tile;
        }
    }
    return nullptr;
}

} // namespace showroom
