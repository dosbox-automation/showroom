// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/legend_bar.h"

#include "ui/theme.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPixmap>
#include <QSignalSpy>
#include <QTest>

namespace showroom {
namespace {

constexpr int kBarWidth = 240;

QImage renderBar(LegendBar& bar)
{
    bar.resize(kBarWidth, theme::kLegendHeightPx);
    QPixmap canvas(bar.size());
    canvas.fill(Qt::black);
    // DrawChildren alone: the default flags include DrawWindowBackground,
    // which fills the whole widget with the palette's window colour and
    // makes every pixel check pass whatever the widget painted.
    bar.render(&canvas, QPoint(), QRegion(), QWidget::DrawChildren);
    return canvas.toImage();
}

// The right-hand end of the bar, where the action button lives. A state
// that offers an action must put something there; a state that does not
// must leave it as it found it.
bool hasInkInActionArea(const QImage& image)
{
    const int left = image.width() - theme::kLegendPaddingXPx - theme::kActionIconPx - 2;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = left; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            // The legend gradient is near-black; an icon is not.
            if (pixel.red() > 60 || pixel.green() > 60 || pixel.blue() > 60) {
                return true;
            }
        }
    }
    return false;
}

TEST(LegendBarPainting, a_state_with_an_action_paints_its_button)
{
    for (const TileState state : {TileState::NotDownloaded,
                                  TileState::Downloaded,
                                  TileState::Ready,
                                  TileState::Running}) {
        LegendBar bar;
        bar.setLicenseLabel(QStringLiteral("SHAREWARE"));
        bar.setState(state);
        EXPECT_TRUE(hasInkInActionArea(renderBar(bar)))
                << "no button drawn for " << tileStateName(state);
    }
}

TEST(LegendBarPainting, a_state_without_an_action_leaves_the_corner_empty)
{
    for (const TileState state : {TileState::Downloading,
                                  TileState::Installing,
                                  TileState::OfflineNotDownloaded,
                                  TileState::NoRecipe}) {
        LegendBar bar;
        bar.setLicenseLabel(QStringLiteral("SHAREWARE"));
        bar.setState(state);
        EXPECT_FALSE(hasInkInActionArea(renderBar(bar)))
                << "unexpected button for " << tileStateName(state);
    }
}

TEST(LegendBarPainting, the_label_gives_way_to_the_phase_while_work_is_running)
{
    LegendBar bar;
    bar.setLicenseLabel(QStringLiteral("SHAREWARE"));
    bar.setState(TileState::Installing);
    bar.setProgress(64);

    const QImage working = renderBar(bar);

    bar.setState(TileState::Ready);
    const QImage idle = renderBar(bar);

    // Different text in the same place: the images must differ.
    EXPECT_NE(working, idle);
}

TEST(LegendBarProgress, a_percentage_outside_the_range_is_clamped_not_trusted)
{
    LegendBar bar;
    bar.setState(TileState::Downloading);

    bar.setProgress(-40);
    const QImage below = renderBar(bar);
    bar.setProgress(0);
    EXPECT_EQ(below, renderBar(bar));

    bar.setProgress(4000);
    const QImage above = renderBar(bar);
    bar.setProgress(100);
    EXPECT_EQ(above, renderBar(bar));
}

TEST(LegendBarInput, clicking_the_button_reports_the_action)
{
    LegendBar bar;
    bar.setState(TileState::Ready);
    bar.resize(kBarWidth, theme::kLegendHeightPx);

    QSignalSpy spy(&bar, &LegendBar::actionTriggered);
    const QPoint on_button(kBarWidth - theme::kLegendPaddingXPx
                                   - theme::kActionIconPx / 2,
                           theme::kLegendHeightPx / 2);
    QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, on_button);

    EXPECT_EQ(spy.count(), 1);
}

TEST(LegendBarInput, clicking_beside_the_button_is_left_for_the_tile)
{
    LegendBar bar;
    bar.setState(TileState::Ready);
    bar.resize(kBarWidth, theme::kLegendHeightPx);

    QSignalSpy spy(&bar, &LegendBar::actionTriggered);
    QTest::mouseClick(&bar,
                      Qt::LeftButton,
                      Qt::NoModifier,
                      QPoint(kBarWidth / 2, theme::kLegendHeightPx / 2));

    EXPECT_EQ(spy.count(), 0);
}

TEST(LegendBarInput, a_state_with_no_action_reports_nothing_wherever_it_is_clicked)
{
    LegendBar bar;
    bar.setState(TileState::NoRecipe);
    bar.resize(kBarWidth, theme::kLegendHeightPx);

    QSignalSpy spy(&bar, &LegendBar::actionTriggered);
    for (const int x : {5, kBarWidth / 2, kBarWidth - 5}) {
        QTest::mouseClick(&bar,
                          Qt::LeftButton,
                          Qt::NoModifier,
                          QPoint(x, theme::kLegendHeightPx / 2));
    }

    EXPECT_EQ(spy.count(), 0);
}

} // namespace
} // namespace showroom
