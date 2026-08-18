// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/tile_grid.h"

#include "ui/game_tile.h"
#include "ui/theme.h"

#include <QGridLayout>
#include <QLabel>

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

    if (tiles_.isEmpty()) {
        // The window opens anyway (a packaging problem must not refuse
        // to start), so the void where the tiles belong says what is
        // wrong and where it looked.
        empty_state_ = new QLabel(this);
        empty_state_->setWordWrap(true);
        empty_state_->setAlignment(Qt::AlignCenter);
        empty_state_->setFont(theme::uiFont(12));
        QPalette message_palette = empty_state_->palette();
        message_palette.setColor(QPalette::WindowText, theme::kMutedText);
        empty_state_->setPalette(message_palette);
        empty_state_->setText(
                tr("No games found in\n%1\n\nThe showroom's game assets are "
                   "missing or unreadable. Reinstalling the showroom should "
                   "restore them.")
                        .arg(QString::fromStdString(games_dir.string())));
        layout->addWidget(empty_state_, 0, 0);
    }
}

void TileGrid::setTileWidth(int width_px)
{
    for (GameTile* tile : tiles_) {
        tile->setTileWidth(width_px);
    }
    if (empty_state_ != nullptr) {
        // Sized to the tile area the step implies, so the message sits
        // centred where the games would be and the window keeps its
        // stepped geometry.
        const int columns = chrome_.columns > 0 ? chrome_.columns : 1;
        const int rows = chrome_.rows > 0 ? chrome_.rows : 1;
        empty_state_->setFixedSize(
                columns * width_px + (columns - 1) * chrome_.gap_px,
                rows * StepSizer::tileHeightFor(width_px)
                        + (rows - 1) * chrome_.gap_px);
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
