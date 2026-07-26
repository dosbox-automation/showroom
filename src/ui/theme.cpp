// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/theme.h"

#include <QFontDatabase>

namespace showroom::theme {

QColor toneColor(TileTone tone)
{
    switch (tone) {
    case TileTone::Idle: return kMutedText;
    case TileTone::Ready: return kGreen;
    case TileTone::Working: return kAmber;
    case TileTone::Disabled: return kDimText;
    }
    return kMutedText;
}

QFont monospaceFont(int point_size, QFont::Weight weight)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(point_size);
    font.setWeight(weight);
    // The mockup tracks its mono labels wide; without it SHAREWARE reads
    // as a word rather than as a tag.
    font.setLetterSpacing(QFont::PercentageSpacing, 108);
    return font;
}

QFont uiFont(int point_size, QFont::Weight weight)
{
    QFont font;
    font.setPointSize(point_size);
    font.setWeight(weight);
    return font;
}

} // namespace showroom::theme
