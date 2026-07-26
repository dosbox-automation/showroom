// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_LEGEND_BAR_H
#define SHOWROOM_UI_LEGEND_BAR_H

#include "model/tile_state.h"

#include <QRect>
#include <QString>
#include <QWidget>

namespace showroom {

// The strip along the bottom of a tile: status dot, license label, and
// the one button the current state offers.
//
// It sits over the lower edge of the screenshot rather than below it and
// fades upward into the image, which is why it paints a gradient instead
// of filling a background.
class LegendBar : public QWidget {
    Q_OBJECT

public:
    explicit LegendBar(QWidget* parent = nullptr);

    void setLicenseLabel(const QString& label);
    void setState(TileState state);

    // 0 to 100. Only shown while the state is a working one; values
    // outside the range are clamped rather than trusted.
    void setProgress(int percent);

    TileState state() const { return state_; }

    QSize sizeHint() const override;

signals:
    // The action button was clicked. Which action that is follows from
    // the state; the tile decides what to do about it.
    void actionTriggered();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // Empty when the state offers no action, which is also what stops
    // the hand cursor appearing over a tile that cannot be clicked.
    QRect actionRect() const;
    QString statusText() const;

    TileState state_ = TileState::NotDownloaded;
    QString license_label_;
    int progress_percent_ = 0;
    bool action_hovered_ = false;
};

} // namespace showroom

#endif // SHOWROOM_UI_LEGEND_BAR_H
