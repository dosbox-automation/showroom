// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/sidebar.h"

#include "ui/theme.h"

#include <QAbstractButton>
#include <QDesktopServices>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QVBoxLayout>

namespace showroom {
namespace {

constexpr int kSidebarTopPaddingPx = 15;
constexpr int kSidebarBottomPaddingPx = 13;
constexpr int kSidebarGapPx = 16;
constexpr int kDividerWidthPx = 38;
constexpr int kBrandTextWidthPx = 20;

// "DOSBOX-AUTOMATION SHOWROOM" running up the strip. Rotated text needs
// its own painting: a QLabel cannot turn a corner.
class BrandText : public QWidget {
public:
    explicit BrandText(QWidget* parent = nullptr) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        // A Fixed policy takes its width from the size hint, and the
        // default hint for a plain QWidget is nothing at all: without
        // this the widget is zero pixels wide and paints into empty
        // space, which is exactly as visible as not being there.
        setFixedWidth(kBrandTextWidthPx);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font = theme::monospaceFont(12, QFont::DemiBold);
        font.setLetterSpacing(QFont::PercentageSpacing, 130);
        painter.setFont(font);
        painter.setPen(theme::kDimText);

        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(-90);
        painter.drawText(QRectF(-height() / 2.0, -width() / 2.0, height(), width()),
                         Qt::AlignCenter,
                         QStringLiteral("DOSBOX-AUTOMATION SHOWROOM"));
    }
};

// A logo in a rounded box. Falls back to the two-letter mark from the
// mockup when the SVG is missing, so a stripped-down build still shows
// something clickable rather than an empty square.
class LogoButton : public QAbstractButton {
public:
    LogoButton(const std::filesystem::path& svg_path, QString fallback, QString tooltip,
               QWidget* parent = nullptr)
            : QAbstractButton(parent),
              fallback_(std::move(fallback))
    {
        setFixedSize(theme::kLogoButtonPx, theme::kLogoButtonPx);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tooltip);

        const QIcon icon(QString::fromStdString(svg_path.string()));
        if (!icon.isNull()) {
            const int inner = theme::kLogoButtonPx - 10;
            logo_ = icon.pixmap(QSize(inner, inner));
        }
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath box;
        box.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        if (underMouse()) {
            painter.fillPath(box, QColor(255, 255, 255, 12));
        }
        painter.setPen(theme::kBorderStrong);
        painter.drawPath(box);

        if (!logo_.isNull()) {
            const QPoint at((width() - logo_.width()) / 2,
                            (height() - logo_.height()) / 2);
            painter.drawPixmap(at, logo_);
            return;
        }

        painter.setFont(theme::monospaceFont(10, QFont::Bold));
        painter.setPen(QColor(255, 255, 255, 199));
        painter.drawText(rect(), Qt::AlignCenter, fallback_);
    }

private:
    QPixmap logo_;
    QString fallback_;
};

// About, Update and Quit: a drawn glyph over a small label.
class SidebarButton : public QAbstractButton {
public:
    enum class Glyph { Info, Update, Power };

    SidebarButton(Glyph glyph, const QString& label, QWidget* parent = nullptr)
            : QAbstractButton(parent),
              glyph_(glyph)
    {
        setText(label);
        setFixedSize(theme::kSidebarButtonWidthPx, 34);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool enabled = isEnabled();
        QColor color = enabled ? theme::kMutedText : theme::kDimText;
        if (enabled && underMouse()) {
            color = theme::kPrimaryText;
        }

        if (underMouse() && enabled) {
            QPainterPath box;
            box.addRoundedRect(QRectF(rect()), 10, 10);
            painter.fillPath(box, QColor(255, 255, 255, 12));
        }

        const QRectF glyph_box((width() - 14) / 2.0, 4, 14, 14);
        QPen pen(color);
        pen.setWidthF(1.4);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        switch (glyph_) {
        case Glyph::Info:
            painter.drawEllipse(glyph_box);
            painter.drawLine(QPointF(glyph_box.center().x(), glyph_box.top() + 5.5),
                             QPointF(glyph_box.center().x(), glyph_box.bottom() - 3));
            painter.drawPoint(QPointF(glyph_box.center().x(), glyph_box.top() + 3.5));
            break;
        case Glyph::Update:
            // An arrow into a tray: the same download gesture as a tile,
            // aimed at the application itself.
            painter.drawLine(QPointF(glyph_box.center().x(), glyph_box.top() + 1),
                             QPointF(glyph_box.center().x(), glyph_box.bottom() - 5));
            painter.drawLine(QPointF(glyph_box.left() + 3, glyph_box.bottom() - 8),
                             QPointF(glyph_box.center().x(), glyph_box.bottom() - 5));
            painter.drawLine(QPointF(glyph_box.right() - 3, glyph_box.bottom() - 8),
                             QPointF(glyph_box.center().x(), glyph_box.bottom() - 5));
            painter.drawLine(QPointF(glyph_box.left() + 1, glyph_box.bottom() - 1),
                             QPointF(glyph_box.right() - 1, glyph_box.bottom() - 1));
            break;
        case Glyph::Power:
            painter.drawArc(glyph_box.adjusted(1, 2, -1, 0), -60 * 16, 300 * 16);
            painter.drawLine(QPointF(glyph_box.center().x(), glyph_box.top()),
                             QPointF(glyph_box.center().x(), glyph_box.center().y() - 1));
            break;
        }

        painter.setFont(theme::uiFont(9, QFont::DemiBold));
        painter.setPen(color);
        painter.drawText(QRect(0, 20, width(), 13), Qt::AlignCenter, text());
    }

private:
    Glyph glyph_;
};

QWidget* makeDivider(QWidget* parent)
{
    auto* line = new QWidget(parent);
    line->setFixedSize(kDividerWidthPx, 1);
    line->setAutoFillBackground(true);
    QPalette palette = line->palette();
    palette.setColor(QPalette::Window, theme::kDividerLine);
    line->setPalette(palette);
    return line;
}

} // namespace

Sidebar::Sidebar(const QString& engine_version, const std::filesystem::path& logos_dir,
                 QWidget* parent)
        : QWidget(parent)
{
    setFixedWidth(78);
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, theme::kSidebarBackground);
    setPalette(background);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, kSidebarTopPaddingPx, 0, kSidebarBottomPaddingPx);
    layout->setSpacing(kSidebarGapPx);
    layout->setAlignment(Qt::AlignHCenter);

    auto* version = new QLabel(engine_version, this);
    version->setAlignment(Qt::AlignCenter);
    version->setFont(theme::monospaceFont(10, QFont::Bold));
    QPalette version_palette = version->palette();
    version_palette.setColor(QPalette::WindowText, theme::kAmber);
    version->setPalette(version_palette);
    layout->addWidget(version, 0, Qt::AlignHCenter);

    layout->addWidget(makeDivider(this), 0, Qt::AlignHCenter);
    layout->addWidget(new BrandText(this), 1, Qt::AlignHCenter);

    auto* logos = new QVBoxLayout;
    logos->setSpacing(6);
    auto* engine_logo = new LogoButton(logos_dir / "dosbox-automation.svg",
                                       QStringLiteral("dA"),
                                       QStringLiteral("dosbox-automation.org"),
                                       this);
    connect(engine_logo, &QAbstractButton::clicked, this, [this] {
        openUrl(QStringLiteral("https://dosbox-automation.org"));
    });
    logos->addWidget(engine_logo, 0, Qt::AlignHCenter);

    auto* luducat_logo = new LogoButton(logos_dir / "luducat.svg",
                                        QStringLiteral("lc"),
                                        QStringLiteral("luducat"),
                                        this);
    connect(luducat_logo, &QAbstractButton::clicked, this, [this] {
        openUrl(QStringLiteral("https://luducat.org"));
    });
    logos->addWidget(luducat_logo, 0, Qt::AlignHCenter);
    layout->addLayout(logos);

    auto* about = new SidebarButton(SidebarButton::Glyph::Info,
                                    QStringLiteral("About"),
                                    this);
    connect(about, &QAbstractButton::clicked, this, &Sidebar::aboutRequested);
    layout->addWidget(about, 0, Qt::AlignHCenter);

    auto* update = new SidebarButton(SidebarButton::Glyph::Update,
                                     QStringLiteral("Update"),
                                     this);
    // Phase 1 ships without an updater. The button stays visible so the
    // sidebar is the one the design describes, and says plainly why it
    // cannot be pressed.
    update->setEnabled(false);
    update->setToolTip(tr("Updates arrive in a later version"));
    connect(update, &QAbstractButton::clicked, this, &Sidebar::updateRequested);
    layout->addWidget(update, 0, Qt::AlignHCenter);

    auto* quit = new SidebarButton(SidebarButton::Glyph::Power,
                                   QStringLiteral("Quit"),
                                   this);
    connect(quit, &QAbstractButton::clicked, this, &Sidebar::quitRequested);
    layout->addWidget(quit, 0, Qt::AlignHCenter);
}

void Sidebar::openUrl(const QString& url) const
{
    QDesktopServices::openUrl(QUrl(url));
}

} // namespace showroom
