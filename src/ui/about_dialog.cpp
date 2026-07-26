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
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace showroom {
namespace {

constexpr int kThumbnailWidthPx = 92;
constexpr int kStripColumns = 8;

QLabel* makeLabel(const QString& text, const QFont& font, const QColor& color,
                  QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(font);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, color);
    label->setPalette(palette);
    return label;
}

// One project: its mark over its name, the whole card clickable. The
// augra mark is Mother's to give and is not in the tree yet, so a card
// without a logo file falls back to its name in the same box.
QWidget* makeProjectCard(const QString& name, const std::filesystem::path& logo_path,
                         const QString& url, QWidget* parent)
{
    auto* card = new QPushButton(parent);
    card->setCursor(Qt::PointingHandCursor);
    card->setFlat(true);
    card->setMinimumHeight(72);
    card->setToolTip(url);
    QObject::connect(card, &QPushButton::clicked, card, [url] {
        QDesktopServices::openUrl(QUrl(url));
    });

    auto* layout = new QVBoxLayout(card);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignCenter);

    const QIcon logo(QString::fromStdString(logo_path.string()));
    if (!logo.isNull() && !logo.availableSizes().isEmpty()) {
        auto* mark = new QLabel(card);
        mark->setPixmap(logo.pixmap(QSize(32, 32)));
        mark->setAlignment(Qt::AlignCenter);
        layout->addWidget(mark);
    }

    layout->addWidget(makeLabel(name,
                                theme::monospaceFont(10, QFont::Bold),
                                theme::kMutedText,
                                card),
                      0,
                      Qt::AlignCenter);
    return card;
}

} // namespace

AboutDialog::AboutDialog(const GameCatalog& catalog,
                         const std::filesystem::path& assets_dir, QWidget* parent)
        : QDialog(parent)
{
    setWindowTitle(tr("About the showroom"));
    setModal(true);
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, theme::kWindowBackground);
    background.setColor(QPalette::WindowText, theme::kPrimaryText);
    setPalette(background);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);

    layout->addWidget(makeLabel(QStringLiteral("dosbox-automation showroom %1")
                                        .arg(QString::fromLatin1(kShowroomVersion)),
                                theme::uiFont(16, QFont::Bold),
                                theme::kPrimaryText,
                                this));

    layout->addWidget(
            makeLabel(tr("Sixteen DOS games that install and run themselves, so the "
                         "automation can be watched instead of described."),
                      theme::uiFont(11),
                      theme::kMutedText,
                      this));

    auto* homepage = new QPushButton(tr("dosbox-automation.org"), this);
    homepage->setFlat(true);
    homepage->setCursor(Qt::PointingHandCursor);
    connect(homepage, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://dosbox-automation.org")));
    });
    layout->addWidget(homepage, 0, Qt::AlignLeft);

    auto* strip = new QGridLayout;
    strip->setSpacing(10);
    int index = 0;
    for (const GameDefinition& definition : catalog) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(4);

        auto* thumbnail = new QLabel(this);
        const std::filesystem::path shot = assets_dir / "games" / definition.slug()
                                         / definition.screenshots().title;
        QPixmap pixmap(QString::fromStdString(shot.string()));
        if (!pixmap.isNull()) {
            thumbnail->setPixmap(pixmap.scaled(kThumbnailWidthPx,
                                               kThumbnailWidthPx * 3 / 4,
                                               Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
        }
        thumbnail->setFixedSize(kThumbnailWidthPx, kThumbnailWidthPx * 3 / 4);
        thumbnail->setAlignment(Qt::AlignCenter);
        cell->addWidget(thumbnail);

        auto* caption = makeLabel(QString::fromStdString(definition.title()),
                                  theme::uiFont(8),
                                  theme::kSubtitleText,
                                  this);
        caption->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        caption->setWordWrap(true);
        caption->setFixedWidth(kThumbnailWidthPx);
        cell->addWidget(caption);

        strip->addLayout(cell, index / kStripColumns, index % kStripColumns);
        ++index;
    }
    layout->addLayout(strip);

    auto* projects = new QHBoxLayout;
    projects->setSpacing(12);
    projects->addWidget(makeProjectCard(QStringLiteral("dosbox-automation"),
                                        assets_dir / "logos" / "dosbox-automation.svg",
                                        QStringLiteral("https://dosbox-automation.org"),
                                        this));
    projects->addWidget(makeProjectCard(QStringLiteral("luducat"),
                                        assets_dir / "logos" / "luducat.svg",
                                        QStringLiteral("https://luducat.org"),
                                        this));
    projects->addWidget(
            makeProjectCard(QStringLiteral("augra engine"),
                            assets_dir / "logos" / "augra.svg",
                            QStringLiteral("https://github.com/augra-project"),
                            this));
    layout->addLayout(projects);

    auto* rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);
    layout->addWidget(rule);

    layout->addWidget(makeLabel(tr("GPL-3.0-or-later. The bundled games keep their own "
                                   "licenses: shareware, freeware and demo releases, "
                                   "redistributed unmodified."),
                                theme::uiFont(9),
                                theme::kDimText,
                                this));

    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(close, 0, Qt::AlignRight);
}

} // namespace showroom
