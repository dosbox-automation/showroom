// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/game_tile.h"

#include "model/step_sizer.h"
#include "ui/legend_bar.h"
#include "ui/theme.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace showroom {
namespace {

QString licenseLabel(License license)
{
    switch (license) {
    case License::Shareware: return QStringLiteral("SHAREWARE");
    case License::Freeware: return QStringLiteral("FREEWARE");
    case License::Demo: return QStringLiteral("DEMO");
    }
    return {};
}

// A screenshot that is not on disk leaves the tile dark rather than
// aborting: the grid is built once, from bundled assets, and one absent
// file is not worth sixteen missing tiles.
QPixmap loadScreenshot(const std::filesystem::path& assets_dir,
                       const std::string& filename)
{
    if (filename.empty()) {
        return {};
    }
    const std::filesystem::path path = assets_dir / filename;
    QPixmap pixmap;
    pixmap.load(QString::fromStdString(path.string()));
    return pixmap;
}

} // namespace

GameTile::GameTile(const GameDefinition& definition,
                   const std::filesystem::path& assets_dir, QWidget* parent)
        : QWidget(parent),
          slug_(QString::fromStdString(definition.slug())),
          title_shot_(loadScreenshot(assets_dir, definition.screenshots().title)),
          gameplay_shot_(loadScreenshot(assets_dir, definition.screenshots().gameplay))
{
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setToolTip(QString::fromStdString(definition.title()));

    legend_ = new LegendBar(this);
    legend_->setLicenseLabel(licenseLabel(definition.license()));
    connect(legend_, &LegendBar::actionTriggered, this, [this] {
        emit actionTriggered(slug_);
    });

    // A definition with no launch executable has no recipe yet, which is
    // the state fifteen of the sixteen start in today.
    state_ = definition.isLaunchable() ? TileState::NotDownloaded : TileState::NoRecipe;
    legend_->setState(state_);
    setCursor(actionFor(state_) == TileAction::None ? Qt::ArrowCursor
                                                    : Qt::PointingHandCursor);
}

void GameTile::setTileWidth(int width_px)
{
    setFixedSize(width_px, StepSizer::tileHeightFor(width_px));
}

void GameTile::setState(TileState state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    legend_->setState(state);
    setCursor(actionFor(state_) == TileAction::None ? Qt::ArrowCursor
                                                    : Qt::PointingHandCursor);
    updateScaledPixmap();
    update();
}

void GameTile::setProgress(int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    if (progress_percent_ == clamped) {
        return;
    }
    progress_percent_ = clamped;
    legend_->setProgress(clamped);
    update();
}

const QPixmap& GameTile::currentPixmap() const
{
    // Running shows the gameplay shot whether or not the pointer is over
    // it: the tile is standing in for a window that is elsewhere.
    const bool wants_gameplay = hovered_ || state_ == TileState::Running;
    if (wants_gameplay && !gameplay_shot_.isNull()) {
        return gameplay_shot_;
    }
    return title_shot_;
}

void GameTile::updateScaledPixmap()
{
    const QPixmap& source = currentPixmap();
    if (source.isNull() || size().isEmpty()) {
        scaled_ = {};
        return;
    }

    const bool grayscale = toneFor(state_) == TileTone::Disabled;
    const bool gameplay = (&source == &gameplay_shot_);
    if (!scaled_.isNull() && scaled_for_size_ == size()
        && scaled_is_grayscale_ == grayscale && scaled_is_gameplay_ == gameplay) {
        return;
    }

    // Rayman and Jazz Jackrabbit were captured in a graphics mode that
    // is not 4:3. The captures are honest and the tile is not, so they
    // letterbox rather than stretch (Stefan, 2026-07-26).
    QPixmap scaled = source.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (grayscale) {
        QImage image = scaled.toImage().convertToFormat(QImage::Format_Grayscale8);
        scaled = QPixmap::fromImage(image);
    }

    scaled_ = scaled;
    scaled_for_size_ = size();
    scaled_is_grayscale_ = grayscale;
    scaled_is_gameplay_ = gameplay;
}

QString GameTile::overlayMessage() const
{
    switch (state_) {
    case TileState::OfflineNotDownloaded: return tr("Internet connection required");
    case TileState::NoRecipe: return tr("Not installable yet - check back later");
    case TileState::Downloading: return tr("DOWNLOADING %1%").arg(progress_percent_);
    case TileState::Installing: return tr("INSTALLING %1%").arg(progress_percent_);
    default: return {};
    }
}

void GameTile::resizeEvent(QResizeEvent* event)
{
    legend_->setGeometry(0,
                         height() - theme::kLegendHeightPx,
                         width(),
                         theme::kLegendHeightPx);
    updateScaledPixmap();
    QWidget::resizeEvent(event);
}

void GameTile::enterEvent(QEnterEvent* event)
{
    hovered_ = true;
    updateScaledPixmap();
    update();
    QWidget::enterEvent(event);
}

void GameTile::leaveEvent(QEvent* event)
{
    hovered_ = false;
    updateScaledPixmap();
    update();
    QWidget::leaveEvent(event);
}

void GameTile::mousePressEvent(QMouseEvent* event)
{
    // Clicking the picture is the same intent as clicking the button,
    // and a state with no action ignores both.
    if (event->button() == Qt::LeftButton && actionFor(state_) != TileAction::None) {
        emit actionTriggered(slug_);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void GameTile::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath frame;
    frame.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                         theme::kTileCornerRadiusPx,
                         theme::kTileCornerRadiusPx);
    painter.setClipPath(frame);

    painter.fillRect(rect(), theme::kSidebarBackground);

    if (!scaled_.isNull()) {
        // Centred, so a capture that is not 4:3 sits between two bars
        // rather than off to one side.
        const QPoint at((width() - scaled_.width()) / 2,
                        (height() - scaled_.height()) / 2);
        painter.drawPixmap(at, scaled_);
    }

    const QString message = overlayMessage();
    if (!message.isEmpty()) {
        painter.fillRect(rect(), theme::kOverlayScrim);

        if (showsProgress(state_)) {
            // The ring is the mockup's spinner standing still. Turning
            // it needs a timer and something real to wait for, which
            // arrives with the download and install phases.
            const int diameter = 26;
            const QRectF ring((width() - diameter) / 2.0,
                              height() / 2.0 - diameter - 6,
                              diameter,
                              diameter);
            QPen track(theme::kProgressTrack);
            track.setWidth(3);
            painter.setPen(track);
            painter.drawEllipse(ring);

            QPen arc(theme::kAmber);
            arc.setWidth(3);
            painter.setPen(arc);
            painter.drawArc(ring, 90 * 16, -progress_percent_ * 360 * 16 / 100);
        }

        painter.setFont(theme::monospaceFont(10, QFont::Bold));
        painter.setPen(showsProgress(state_) ? theme::kBrightText : theme::kMutedText);
        const QRect text_area = rect().adjusted(10, 0, -10, -theme::kLegendHeightPx);
        painter.drawText(text_area, Qt::AlignCenter | Qt::TextWordWrap, message);
    }

    if (showsProgress(state_)) {
        const QRect track(0,
                          height() - theme::kProgressHeightPx,
                          width(),
                          theme::kProgressHeightPx);
        painter.fillRect(track, theme::kProgressTrack);
        QRect fill = track;
        fill.setWidth(track.width() * progress_percent_ / 100);
        painter.fillRect(fill, theme::kAmber);
    }

    painter.setClipping(false);
    painter.setPen(theme::kBorderSubtle);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(frame);
}

} // namespace showroom
