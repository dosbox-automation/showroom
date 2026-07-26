// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/legend_bar.h"

#include "ui/theme.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace showroom {
namespace {

// The action icons are three shapes, so they are drawn rather than
// shipped as assets: no icon file can follow the accent colour of a
// state the way a path filled with the current brush does.
void drawPlayTriangle(QPainter& painter, const QRectF& box, const QColor& color)
{
    QPainterPath path;
    path.moveTo(box.left(), box.top());
    path.lineTo(box.right(), box.center().y());
    path.lineTo(box.left(), box.bottom());
    path.closeSubpath();
    painter.fillPath(path, color);
}

void drawDownloadArrow(QPainter& painter, const QRectF& box, const QColor& color)
{
    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const qreal shaft_bottom = box.bottom() - box.height() * 0.28;
    painter.drawLine(QPointF(box.center().x(), box.top()),
                     QPointF(box.center().x(), shaft_bottom));
    painter.drawLine(QPointF(box.left() + box.width() * 0.18,
                             shaft_bottom - box.height() * 0.22),
                     QPointF(box.center().x(), shaft_bottom));
    painter.drawLine(QPointF(box.right() - box.width() * 0.18,
                             shaft_bottom - box.height() * 0.22),
                     QPointF(box.center().x(), shaft_bottom));
    painter.drawLine(QPointF(box.left(), box.bottom()),
                     QPointF(box.right(), box.bottom()));
}

void drawStopSquare(QPainter& painter, const QRectF& box, const QColor& color)
{
    QPainterPath path;
    path.addRoundedRect(box.adjusted(1, 1, -1, -1), 2, 2);
    painter.fillPath(path, color);
}

} // namespace

LegendBar::LegendBar(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setFixedHeight(theme::kLegendHeightPx);
    // setState early-returns when the state has not changed, so the
    // starting state needs its cursor applied here or a fresh tile keeps
    // the arrow over a button that works.
    setCursor(actionRect().isNull() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void LegendBar::setLicenseLabel(const QString& label)
{
    if (license_label_ == label) {
        return;
    }
    license_label_ = label;
    update();
}

void LegendBar::setState(TileState state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    action_hovered_ = false;
    setCursor(actionRect().isNull() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void LegendBar::setProgress(int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    if (progress_percent_ == clamped) {
        return;
    }
    progress_percent_ = clamped;
    update();
}

QSize LegendBar::sizeHint() const
{
    return {0, theme::kLegendHeightPx};
}

QString LegendBar::statusText() const
{
    // While work is in progress the label gives up its place to the
    // phase, which is the only moment a tile has something to say that
    // its licence does not.
    switch (state_) {
    case TileState::Downloading:
        return QStringLiteral("DOWNLOADING %1%").arg(progress_percent_);
    case TileState::Installing:
        return QStringLiteral("INSTALLING %1%").arg(progress_percent_);
    default: return license_label_;
    }
}

QRect LegendBar::actionRect() const
{
    if (actionFor(state_) == TileAction::None) {
        return {};
    }

    const int size = theme::kActionIconPx;
    const int x = width() - theme::kLegendPaddingXPx - size;
    const int y = (height() - size) / 2;
    return {x, y, size, size};
}

void LegendBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Three stops rather than the mockup's two: over flat placeholder
    // tiles a half-transparent band reads fine, over DOOM's id logo it
    // swallowed the button whole.
    QLinearGradient gradient(0, height(), 0, 0);
    gradient.setColorAt(0.0, theme::kLegendGradientBottom);
    gradient.setColorAt(0.6, theme::kLegendGradientMiddle);
    gradient.setColorAt(1.0, QColor(9, 9, 12, 0));
    painter.fillRect(rect(), gradient);

    const QColor tone = theme::toneColor(toneFor(state_));

    const int dot_y = (height() - theme::kStatusDotPx) / 2;
    QPainterPath dot;
    dot.addRoundedRect(QRectF(theme::kLegendPaddingXPx,
                              dot_y,
                              theme::kStatusDotPx,
                              theme::kStatusDotPx),
                       theme::kStatusDotRadiusPx,
                       theme::kStatusDotRadiusPx);
    painter.fillPath(dot, tone);

    painter.setFont(theme::monospaceFont(9, QFont::Bold));
    painter.setPen(tone);
    const int text_left = theme::kLegendPaddingXPx + theme::kStatusDotPx + 5;
    const QRect text_area(text_left,
                          0,
                          width() - text_left - theme::kLegendPaddingXPx * 2,
                          height());
    painter.drawText(text_area,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(statusText(),
                                                      Qt::ElideRight,
                                                      text_area.width()));

    const QRect action = actionRect();
    if (action.isNull()) {
        return;
    }

    // The mockup gives the play triangle its own green and leaves the
    // download arrow in the muted text colour; hover lifts whichever it
    // is rather than changing the shape.
    QColor action_color = theme::kMutedText;
    switch (actionFor(state_)) {
    case TileAction::Play: action_color = theme::kGreen; break;
    case TileAction::Stop: action_color = theme::kRed; break;
    case TileAction::Download:
    case TileAction::None: break;
    }
    if (action_hovered_) {
        action_color = action_color.lighter(120);
    }

    const QRectF box(action);
    switch (actionFor(state_)) {
    case TileAction::Play:
        drawPlayTriangle(painter, box.adjusted(1, 0, -1, 0), action_color);
        break;
    case TileAction::Download: drawDownloadArrow(painter, box, action_color); break;
    case TileAction::Stop: drawStopSquare(painter, box, action_color); break;
    case TileAction::None: break;
    }
}

void LegendBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && actionRect().contains(event->pos())) {
        emit actionTriggered();
        event->accept();
        return;
    }
    // Anything else belongs to the tile: clicking the image is the same
    // gesture as clicking the button.
    event->ignore();
}

void LegendBar::mouseMoveEvent(QMouseEvent* event)
{
    const bool hovered = actionRect().contains(event->pos());
    if (hovered != action_hovered_) {
        action_hovered_ = hovered;
        update();
    }
    event->ignore();
}

void LegendBar::leaveEvent(QEvent* event)
{
    if (action_hovered_) {
        action_hovered_ = false;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace showroom
