// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_THEME_H
#define SHOWROOM_UI_THEME_H

#include "model/tile_state.h"

#include <QColor>
#include <QFont>

namespace showroom::theme {

// Taken from the mockup's stylesheet (developer-docs
// design/dosbox-automation-showroom/DOS Launcher.dc.html), not sampled
// off the rendered PNG. Its oklch accents are converted to sRGB here.

inline const QColor kWindowBackground{0x13, 0x14, 0x17};
inline const QColor kSidebarBackground{0x19, 0x1a, 0x1e};
inline const QColor kPrimaryText{0xe7, 0xe7, 0xea};

inline const QColor kBorderSubtle{255, 255, 255, 20}; // .08
inline const QColor kBorderStrong{255, 255, 255, 41}; // .16
inline const QColor kDividerLine{255, 255, 255, 20};  // .08

inline const QColor kMutedText{255, 255, 255, 140};   // .55
inline const QColor kDimText{255, 255, 255, 77};      // .3
inline const QColor kBrightText{255, 255, 255, 240};  // .94
inline const QColor kSubtitleText{255, 255, 255, 92}; // .36

// oklch(0.83 0.15 78) - the version badge and everything in progress.
inline const QColor kAmber{0xfc, 0xba, 0x43};
// oklch(0.82 0.14 150) - installed and ready.
inline const QColor kGreen{0x7c, 0xdd, 0x93};
// oklch(0.75 0.19 25) - the stop button on a running game.
inline const QColor kRed{0xff, 0x74, 0x6e};

// The legend bar sits over the lower edge of the screenshot and fades
// upward into it.
inline const QColor kLegendGradientBottom{9, 9, 12, 235}; // .92
inline const QColor kLegendGradientMiddle{9, 9, 12, 190}; // ours, see legend_bar.cpp
inline const QColor kOverlayScrim{8, 9, 12, 189};         // .74
inline const QColor kProgressTrack{255, 255, 255, 31};    // .12

inline constexpr int kTileCornerRadiusPx = 10;
inline constexpr int kLegendHeightPx = 30;
inline constexpr int kLegendPaddingXPx = 10;
inline constexpr int kProgressHeightPx = 5;
inline constexpr int kStatusDotPx = 6;
inline constexpr int kStatusDotRadiusPx = 2;
inline constexpr int kActionIconPx = 12;
inline constexpr int kLogoButtonPx = 31;
inline constexpr int kSidebarButtonWidthPx = 56;

// Stefan's call, 2026-07-26: the dot and label follow the STATE, per the
// design document, where the mockup colours them by license instead.
QColor toneColor(TileTone tone);

// The mockup's Space Grotesk and IBM Plex Mono cannot be assumed
// present; bundling them is a phase 8 decision.
QFont monospaceFont(int point_size, QFont::Weight weight = QFont::Normal);
QFont uiFont(int point_size, QFont::Weight weight = QFont::Normal);

} // namespace showroom::theme

#endif // SHOWROOM_UI_THEME_H
