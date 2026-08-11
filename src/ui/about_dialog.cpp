// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/about_dialog.h"

#include "ui/theme.h"
#include "ui/version.h"

#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace showroom {
namespace {

const QColor kAccent{0x2a, 0x82, 0xda};
const QColor kBadgeText{0x7a, 0xb5, 0xea};
const QColor kBodyText{255, 255, 255, 158};
const QColor kSectionText{255, 255, 255, 115};
const QColor kCountText{255, 255, 255, 77};
const QColor kCaptionText{255, 255, 255, 168};
const QColor kCardName{255, 255, 255, 230};
const QColor kCardDesc{255, 255, 255, 115};
const QColor kLicenseText{255, 255, 255, 107};
const QColor kDivider{255, 255, 255, 26};
const QColor kLinkText{255, 255, 255, 184};

constexpr int kDialogWidth = 900;
constexpr int kIconPx = 64;
constexpr int kStackIconPx = 26;
constexpr int kThumbWidth = 92;
constexpr int kThumbHeight = 69;
constexpr int kStripColumns = 8;

QLabel* makeLabel(const QString& text, const QFont& font, const QColor& color,
                  QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(font);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, color);
    label->setPalette(pal);
    return label;
}

QWidget* makeSectionHeader(const QString& title, const QString& count, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QFont section_font = theme::uiFont(9, QFont::Bold);
    section_font.setLetterSpacing(QFont::PercentageSpacing, 109);
    section_font.setCapitalization(QFont::AllUppercase);
    layout->addWidget(makeLabel(title, section_font, kSectionText, row));

    if (!count.isEmpty()) {
        layout->addWidget(
                makeLabel(count, theme::monospaceFont(9, QFont::Medium), kCountText, row));
    }

    auto* line = new QFrame(row);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(1);
    QPalette line_pal = line->palette();
    line_pal.setColor(QPalette::WindowText, kDivider);
    line->setPalette(line_pal);
    layout->addWidget(line, 1);

    return row;
}

QPushButton* makeLinkButton(const QString& text, const QString& url, bool accent,
                            QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setFont(theme::uiFont(10));

    const QColor fg = accent ? kBadgeText : kLinkText;
    btn->setStyleSheet(
            QString("QPushButton { color: %1; background: rgba(255,255,255,0.05);"
                    " border: 1px solid rgba(255,255,255,0.1);"
                    " border-radius: 3px; padding: 6px 11px; }"
                    "QPushButton:hover { background: rgba(255,255,255,0.1); }")
                    .arg(fg.name()));

    QObject::connect(btn, &QPushButton::clicked, btn, [url] {
        QDesktopServices::openUrl(QUrl(url));
    });
    return btn;
}

// A QPushButton card clips its children: QPushButton::sizeHint() is
// computed from the button's own text and ignores child layouts, so the
// word-wrapped blurb never gets room. A QFrame sizes from its layout.
class ProjectCard : public QFrame {
public:
    ProjectCard(const QString& url, QWidget* parent) : QFrame(parent), url_(url)
    {
        setCursor(Qt::PointingHandCursor);
        // The selector must not use the QFrame class name: QLabel derives
        // from QFrame, so the card children would inherit the box.
        setObjectName(QStringLiteral("projectCard"));
        setStyleSheet(
                QString("#projectCard { background: %1;"
                        " border: 1px solid rgba(255,255,255,0.09);"
                        " border-radius: 3px; padding: 12px 13px; }"
                        "#projectCard:hover { border-color: rgba(255,255,255,0.18); }")
                        .arg(theme::kSidebarBackground.name()));
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            QDesktopServices::openUrl(QUrl(url_));
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QString url_;
};

QWidget* makeProjectCard(const QString& name, const std::filesystem::path& logo_path,
                         const QString& url, const QString& blurb, QWidget* parent)
{
    auto* card = new ProjectCard(url, parent);

    auto* layout = new QHBoxLayout(card);
    layout->setSpacing(11);
    layout->setContentsMargins(0, 0, 0, 0);

    const QIcon logo(QString::fromStdString(logo_path.string()));
    if (!logo.isNull() && !logo.availableSizes().isEmpty()) {
        auto* mark = new QLabel(card);
        mark->setPixmap(logo.pixmap(QSize(kStackIconPx, kStackIconPx)));
        mark->setFixedSize(kStackIconPx, kStackIconPx);
        layout->addWidget(mark, 0, Qt::AlignTop);
    }

    auto* text_col = new QVBoxLayout;
    text_col->setSpacing(3);

    auto* title = new QLabel(name, card);
    title->setFont(theme::uiFont(10, QFont::Bold));
    title->setStyleSheet(QString("color: %1; background: transparent;")
                                 .arg(kCardName.name(QColor::HexArgb)));
    text_col->addWidget(title);

    auto* desc = new QLabel(blurb, card);
    desc->setFont(theme::uiFont(9));
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color: %1; background: transparent;")
                                .arg(kCardDesc.name(QColor::HexArgb)));
    text_col->addWidget(desc);
    text_col->addStretch(1);

    layout->addLayout(text_col, 1);

    return card;
}

} // namespace

AboutDialog::AboutDialog(const GameCatalog& catalog,
                         const std::filesystem::path& assets_dir, QWidget* parent)
        : QDialog(parent)
{
    setWindowTitle(tr("About dosbox-automation showroom"));
    setModal(true);
    setFixedWidth(kDialogWidth);
    setAutoFillBackground(true);
    QPalette bg = palette();
    bg.setColor(QPalette::Window, theme::kWindowBackground);
    bg.setColor(QPalette::WindowText, theme::kPrimaryText);
    setPalette(bg);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* content = new QWidget(this);
    auto* body = new QVBoxLayout(content);
    body->setContentsMargins(34, 30, 34, 26);
    body->setSpacing(26);

    auto* header = new QHBoxLayout;
    header->setSpacing(18);

    auto* icon = new QLabel(content);
    const QIcon app_icon(QString::fromStdString(
            (assets_dir / "logos" / "dosbox-automation.png").string()));
    if (!app_icon.isNull() && !app_icon.availableSizes().isEmpty()) {
        icon->setPixmap(app_icon.pixmap(QSize(kIconPx, kIconPx)));
    }
    icon->setFixedSize(kIconPx, kIconPx);
    icon->setStyleSheet(
            "border: 1px solid rgba(255,255,255,0.12);"
            " border-radius: 6px;");
    header->addWidget(icon, 0, Qt::AlignTop);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(7);

    auto* title_row = new QHBoxLayout;
    title_row->setSpacing(10);
    title_row->addWidget(makeLabel(QStringLiteral("dosbox-automation showroom"),
                                   theme::uiFont(18, QFont::Bold),
                                   theme::kPrimaryText,
                                   content));

    auto* badge = new QLabel(QString::fromLatin1(kShowroomVersion), content);
    badge->setFont(theme::monospaceFont(9, QFont::Medium));
    badge->setStyleSheet(QString("color: %1; background: rgba(42,130,218,0.2);"
                                 " border: 1px solid rgba(42,130,218,0.45);"
                                 " border-radius: 3px; padding: 3px 7px;")
                                 .arg(kBadgeText.name()));
    title_row->addWidget(badge);
    title_row->addStretch();
    title_col->addLayout(title_row);

    auto* desc = makeLabel(tr("A selection of classic shareware and freeware DOS games of the 1990s, "
                              "showcasing the automation capabilities of the dosbox-automation DOS emulator runtime."),
                           theme::uiFont(11),
                           kBodyText,
                           content);
    desc->setWordWrap(true);
    desc->setMaximumWidth(600);
    title_col->addWidget(desc);

    auto* links = new QHBoxLayout;
    links->setSpacing(8);
    links->addWidget(makeLinkButton(QStringLiteral("dosbox-automation.org"),
                                    QStringLiteral("https://dosbox-automation.org"),
                                    true,
                                    content));
    links->addWidget(makeLinkButton(
            QStringLiteral("Source code"),
            QStringLiteral("https://github.com/dosbox-automation/dosbox-automation"),
            false,
            content));
    links->addWidget(makeLinkButton(
            QStringLiteral("Report an issue"),
            QStringLiteral(
                    "https://github.com/dosbox-automation/dosbox-automation/issues"),
            false,
            content));
    links->addStretch();
    title_col->addLayout(links);

    header->addLayout(title_col, 1);
    body->addLayout(header);

    body->addWidget(makeSectionHeader(tr("Bundled games"),
                                      QString::number(catalog.size()),
                                      content));

    auto* grid = new QGridLayout;
    grid->setSpacing(12);
    int index = 0;
    for (const GameDefinition& game : catalog) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(7);

        auto* thumb = new QLabel(content);
        thumb->setFixedSize(kThumbWidth, kThumbHeight);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setStyleSheet(QString("border: 1px solid rgba(255,255,255,0.14);"
                                     " border-radius: 2px; background: %1;")
                                     .arg(theme::kSidebarBackground.name()));

        const std::filesystem::path shot = assets_dir / "games" / game.slug()
                                         / game.screenshots().title;
        QPixmap pm(QString::fromStdString(shot.string()));
        if (!pm.isNull()) {
            thumb->setPixmap(pm.scaled(kThumbWidth - 2,
                                       kThumbHeight - 2,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
        }
        cell->addWidget(thumb);

        auto* caption = makeLabel(QString::fromStdString(game.title()),
                                  theme::uiFont(8),
                                  kCaptionText,
                                  content);
        caption->setWordWrap(true);
	caption->setAlignment(Qt::AlignHCenter);
        caption->setFixedWidth(kThumbWidth);
        cell->addWidget(caption);

        grid->addLayout(cell, index / kStripColumns, index % kStripColumns);
        ++index;
    }
    body->addLayout(grid);

    body->addWidget(makeSectionHeader(tr("Sister projects"), {}, content));

    auto* cards = new QHBoxLayout;
    cards->setSpacing(10);
    cards->addWidget(
            makeProjectCard(
                    QStringLiteral("dosbox-automation"),
                    assets_dir / "logos" / "dosbox-automation.png",
                    QStringLiteral("https://dosbox-automation.org"),
                    tr("DOSBox with a REST API and Lua scripting. Installs, configures "
                       "and runs DOS games unattended."),
                    content),
            1);
    cards->addWidget(
            makeProjectCard(
                    QStringLiteral("augra engine"),
                    assets_dir / "logos" / "augra.png",
                    QStringLiteral("https://github.com/augra-project"),
                    tr("Faithful recreation engine for classic first-person tile-based "
                       "CRPGs of the 1980s and 1990s. Every platform as it looked. "
                       "In development."),
                    content),
            1);
    cards->addWidget(
            makeProjectCard(
                    QStringLiteral("luducat"),
                    assets_dir / "logos" / "luducat.png",
                    QStringLiteral("https://luducat.org"),
                    tr("Multi-store game catalog browser. No accounts, no telemetry, "
                       "works offline. Your library, your rules."),
                    content),
            1);
    body->addLayout(cards);

    outer->addWidget(content, 1);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedHeight(1);
    QPalette sep_pal = sep->palette();
    sep_pal.setColor(QPalette::WindowText, kDivider);
    sep->setPalette(sep_pal);
    outer->addWidget(sep);

    auto* footer = new QWidget(this);
    footer->setAutoFillBackground(true);
    QPalette footer_pal = footer->palette();
    footer_pal.setColor(QPalette::Window, theme::kSidebarBackground);
    footer->setPalette(footer_pal);

    auto* footer_row = new QHBoxLayout(footer);
    footer_row->setContentsMargins(34, 12, 34, 12);
    footer_row->setSpacing(24);

    auto* license = makeLabel(
            tr("GPL-3.0-or-later. Bundled games keep their own licenses - "
               "shareware, freeware and demo releases, redistributed unmodified."),
            theme::uiFont(9),
            kLicenseText,
            footer);
    license->setWordWrap(true);
    footer_row->addWidget(license, 1);

    auto* close = new QPushButton(tr("Close"), footer);
    close->setMinimumWidth(88);
    close->setCursor(Qt::PointingHandCursor);
    close->setFont(theme::uiFont(10));
    close->setStyleSheet(QString("QPushButton { color: #fff; background: %1;"
                                 " border: 1px solid rgba(0,0,0,0.35);"
                                 " border-radius: 3px; padding: 7px 18px; }"
                                 "QPushButton:hover { background: %2; }")
                                 .arg(kAccent.name(), kAccent.lighter(115).name()));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer_row->addWidget(close, 0, Qt::AlignVCenter);

    outer->addWidget(footer);

    // Qt's adjusted-size estimate evaluates height-for-width at the hint
    // width, not at the fixed dialog width, and the shortfall squeezes
    // the wrapped labels. Pin the height at the real width instead.
    outer->activate();
    setFixedHeight(outer->totalHeightForWidth(kDialogWidth));
}

} // namespace showroom
