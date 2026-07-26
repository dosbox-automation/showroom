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

// Sits over the lower edge of the screenshot rather than below it, which
// is why it paints a gradient instead of filling a background.
class LegendBar : public QWidget {
    Q_OBJECT

public:
    explicit LegendBar(QWidget* parent = nullptr);

    void setLicenseLabel(const QString& label);
    void setState(TileState state);

    // Clamped to 0..100 rather than trusted.
    void setProgress(int percent);

    TileState state() const { return state_; }

    QSize sizeHint() const override;

signals:
    // Which action it was follows from the state; the tile decides.
    void actionTriggered();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // Empty when the state offers no action, which also keeps the hand
    // cursor off a tile that cannot be clicked.
    QRect actionRect() const;
    QString statusText() const;

    TileState state_ = TileState::NotDownloaded;
    QString license_label_;
    int progress_percent_ = 0;
    bool action_hovered_ = false;
};

} // namespace showroom

#endif // SHOWROOM_UI_LEGEND_BAR_H
