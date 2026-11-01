/***************************************************************************
 *   Maze Terminal — the window around the terminal                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "chrome.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>

#ifdef HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#endif

namespace Maze {

namespace {

const QColor kText(0xf2, 0xf2, 0xf4);
const QColor kGrey(0x8a, 0x8a, 0x90);
const QColor kDim(0x5a, 0x5a, 0x60);
const int kEdge = 6;
const int kRadius = 12;

const char *kStyleSheet = R"(
QDialog { background: #000000; }
QMainWindow { background: transparent; }

/* The window: a glass pane (see Chrome::setGlass) with a hairline edge. */
#MazeRoot {
    background: rgba(6, 6, 8, 0.86);
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 12px;
}
#MazeRoot[glass="false"] { background: #0a0a0b; }
#MazeRoot[maximized="true"] { border: none; border-radius: 0px; }

/* B: the terminal sits in a card. */
#MazeCard {
    background: #000000;
    border: 1px solid rgba(255, 255, 255, 0.09);
    border-radius: 12px;
}
#MazeCard[flat="true"] { border: none; border-radius: 0px; }

/* Footer chips: frosted glass. */
QLabel#MazeChip {
    color: #8a8a90;
    background: rgba(255, 255, 255, 0.045);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    padding: 3px 10px;
    font-size: 8.5pt;
}
QLabel#MazeChipMode {
    color: #000000;
    background: #f2f2f4;
    border: 1px solid #f2f2f4;
    border-radius: 8px;
    padding: 3px 10px;
    font-size: 8pt;
    font-weight: 700;
    letter-spacing: 1px;
}

/* The drop-down terminal keeps the plain tab bar: Maze chips on black. */
#consoleTabulator::pane { border: none; }
#consoleTabulator QTabBar { background: #000000; border: none; }
#consoleTabulator QTabBar::tab {
    background: transparent;
    color: #8a8a90;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 0px 8px 0px 12px;
    height: 26px;
    margin: 5px 2px;
}
#consoleTabulator QTabBar::tab:hover:!selected { color: #d8d8dc; background: #141416; }
#consoleTabulator QTabBar::tab:selected { color: #f2f2f4; background: #1c1c1f; border-color: #2a2a2e; }
#consoleTabulator QTabBar::close-button { image: url(:/maze/close.svg); subcontrol-position: right; }
#consoleTabulator QTabBar::close-button:hover { image: url(:/maze/close-hover.svg); }
#consoleTabulator QToolButton { background: transparent; border: none; }

/* A: side panel. */
#MazeSide { border-right: 1px solid rgba(255, 255, 255, 0.07); }
#MazeSideTop { border-bottom: 1px solid rgba(255, 255, 255, 0.07); }
#MazeSideStatus { border-top: 1px solid rgba(255, 255, 255, 0.07); }
QLabel#MazeBrand { color: #f2f2f4; font-size: 8.5pt; font-weight: 700; letter-spacing: 3px; }
QLabel#MazeSection { color: #5a5a60; font-size: 7.5pt; font-weight: 600; letter-spacing: 2px; padding: 0px 8px; }
QLabel#MazeCrumb { color: #8a8a90; font-size: 9pt; }
QLabel#MazeStatusText { color: #7a7a80; font-size: 8.5pt; padding: 0px 14px; }
QScrollArea#MazeSessionsArea, QScrollArea#MazeSessionsArea > QWidget > QWidget { background: transparent; border: none; }
QPushButton#MazeSideButton {
    background: rgba(255, 255, 255, 0.04);
    color: #bdbdc2;
    border: 1px solid rgba(255, 255, 255, 0.09);
    border-radius: 8px;
    padding: 7px 0px;
    font-size: 8.5pt;
}
QPushButton#MazeSideButton:hover { background: rgba(255, 255, 255, 0.09); color: #f2f2f4; }
QPushButton#MazeSidePrimary {
    background: #f2f2f4;
    color: #000000;
    border: 1px solid #f2f2f4;
    border-radius: 8px;
    padding: 7px 0px;
    font-size: 8.5pt;
    font-weight: 600;
}
QPushButton#MazeSidePrimary:hover { background: #ffffff; }

QMenuBar { background: #000000; color: #9a9a9a; border-bottom: 1px solid #141414; }
QMenuBar::item { padding: 6px 10px; background: transparent; }
QMenuBar::item:selected { color: #f2f2f4; background: #141414; }

QMenu {
    background: #0b0b0b;
    color: #f2f2f4;
    border: 1px solid #2c2c2c;
    padding: 6px;
}
QMenu::item { padding: 6px 28px 6px 12px; border-radius: 6px; background: transparent; }
QMenu::item:selected { background: #1c1c1c; }
QMenu::item:disabled { color: #4a4a4a; }
QMenu::separator { height: 1px; background: #1c1c1c; margin: 6px 8px; }
QMenu::icon { padding-left: 8px; }

QToolTip {
    background: #0b0b0b;
    color: #f2f2f4;
    border: 1px solid #2c2c2c;
    padding: 6px 8px;
}

QPushButton {
    background: rgba(255, 255, 255, 0.06);
    color: #f2f2f4;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 10px;
    padding: 7px 16px;
}
QPushButton:hover { background: rgba(255, 255, 255, 0.11); border-color: rgba(255, 255, 255, 0.24); }
QPushButton:pressed { background: rgba(255, 255, 255, 0.03); }
QPushButton:default, QPushButton:focus { background: #f2f2f4; color: #000000; border-color: #f2f2f4; }
QPushButton:default:hover { background: #ffffff; }
QPushButton:disabled { color: #4a4a4a; border-color: rgba(255, 255, 255, 0.05); background: transparent; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QKeySequenceEdit, QFontComboBox {
    background: #0b0b0b;
    color: #f2f2f4;
    border: 1px solid #2c2c2c;
    border-radius: 8px;
    padding: 5px 8px;
    selection-background-color: #f2f2f4;
    selection-color: #000000;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #8a8a8a; }
QComboBox QAbstractItemView { background: #0b0b0b; border: 1px solid #2c2c2c; selection-background-color: #1c1c1c; }

QListView, QListWidget, QTreeView, QTreeWidget, QTableView, QTableWidget, QTextEdit, QPlainTextEdit {
    background: #0b0b0b;
    color: #f2f2f4;
    border: 1px solid #1c1c1c;
    border-radius: 8px;
    selection-background-color: #1c1c1c;
    selection-color: #ffffff;
}
QHeaderView::section { background: #0b0b0b; color: #8a8a8a; border: none; border-bottom: 1px solid #1c1c1c; padding: 5px; }

QGroupBox { border: 1px solid #1c1c1c; border-radius: 10px; margin-top: 14px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0px 4px; color: #8a8a8a; }

QDialog QTabWidget::pane { border: 1px solid #1c1c1c; border-radius: 10px; top: -1px; }
QDialog QTabBar::tab { background: transparent; color: #8a8a8a; padding: 7px 14px; border: none; }
QDialog QTabBar::tab:selected { color: #f2f2f4; border-bottom: 1px solid #f2f2f4; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0px; }
QScrollBar::handle:vertical { background: #2c2c2c; border-radius: 3px; min-height: 32px; margin: 2px 3px; }
QScrollBar::handle:horizontal { background: #2c2c2c; border-radius: 3px; min-width: 32px; margin: 3px 2px; }
QScrollBar::handle:hover { background: #4a4a4a; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QDockWidget { color: #9a9a9a; }
QDockWidget::title { background: #000000; padding: 6px; border-bottom: 1px solid #141414; }
QSplitter::handle { background: #141414; })";

void drawPill(QPainter &p, const QRectF &r, const QString &text, bool solid, int pixelSize = 9)
{
    QFont f(QStringLiteral("Inter"));
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (solid)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(kText);
    }
    else
    {
        p.setPen(QPen(QColor(0xa8, 0xa8, 0xae), 1));
        p.setBrush(Qt::NoBrush);
    }
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), r.height() / 2, r.height() / 2);
    p.setFont(f);
    p.setPen(solid ? QColor(Qt::black) : kText);
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
}

qreal pillWidth(const QString &text, int pixelSize = 9)
{
    QFont f(QStringLiteral("Inter"));
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
    return QFontMetricsF(f).horizontalAdvance(text) + 16;
}

} // namespace

void applyTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor black(0, 0, 0), surface(0x0b, 0x0b, 0x0b), elevated(0x14, 0x14, 0x14),
                 dim(0x4a, 0x4a, 0x4a);
    p.setColor(QPalette::Window, black);
    p.setColor(QPalette::WindowText, kText);
    p.setColor(QPalette::Base, surface);
    p.setColor(QPalette::AlternateBase, elevated);
    p.setColor(QPalette::Text, kText);
    p.setColor(QPalette::Button, elevated);
    p.setColor(QPalette::ButtonText, kText);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, kText);
    p.setColor(QPalette::HighlightedText, black);
    p.setColor(QPalette::ToolTipBase, surface);
    p.setColor(QPalette::ToolTipText, kText);
    p.setColor(QPalette::PlaceholderText, dim);
    p.setColor(QPalette::Link, kText);
    p.setColor(QPalette::Mid, QColor(0x2c, 0x2c, 0x2c));
    p.setColor(QPalette::Dark, black);
    p.setColor(QPalette::Light, QColor(0x2c, 0x2c, 0x2c));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, dim);
    app.setPalette(p);

    // The UI font is set on the application, not in the style sheet: a
    // style sheet font would also replace the terminal's own monospace one.
    QFont ui(QStringLiteral("Inter"));
    ui.setPointSizeF(10);
    app.setFont(ui);
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));
}

QSize badgeSize()
{
    return QSize(64, 16);
}

QIcon badge(const QString &text, bool solid)
{
    const QSize size = badgeSize();
    const qreal dpr = 2.0;
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawPill(p, QRectF(0, 0, qMin<qreal>(size.width(), pillWidth(text)), size.height()), text, solid);
    p.end();
    return QIcon(pm);
}

// ── GlyphButton ───────────────────────────────────────────────────────────

GlyphButton::GlyphButton(Glyph glyph, QWidget *parent)
    : QAbstractButton(parent), m_glyph(glyph)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
}

void GlyphButton::setGlyph(Glyph glyph)
{
    m_glyph = glyph;
    update();
}

QSize GlyphButton::sizeHint() const
{
    switch (m_glyph)
    {
        case Minimize: case Maximize: case Restore: case Close:
            return QSize(42, 34);
        default:
            return QSize(34, 34);
    }
}

void GlyphButton::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(e);
}

void GlyphButton::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(e);
}

void GlyphButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    const bool closeHover = m_glyph == Close && m_hover;
    p.setPen(m_framed ? QPen(QColor(255, 255, 255, 26), 1) : Qt::NoPen);
    if (closeHover)
        p.setBrush(kText);
    else if (m_hover || isDown())
        p.setBrush(QColor(255, 255, 255, isDown() ? 12 : 22));
    else
        p.setBrush(m_framed ? QColor(255, 255, 255, 8) : QColor(Qt::transparent));
    p.drawRoundedRect(box, 9, 9);

    QColor ink = closeHover ? QColor(Qt::black) : (m_hover ? kText : QColor(0xbd, 0xbd, 0xc2));
    if (!isEnabled())
        ink = QColor(0x4a, 0x4a, 0x4a);
    QPen pen(ink, 1.3);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QPointF c = QRectF(rect()).center();
    switch (m_glyph)
    {
        case Plus:
            p.drawLine(QPointF(c.x() - 6, c.y()), QPointF(c.x() + 6, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - 6), QPointF(c.x(), c.y() + 6));
            break;
        case Chevron:
        {
            QPainterPath path;
            path.moveTo(c.x() - 4, c.y() - 2);
            path.lineTo(c.x(), c.y() + 2);
            path.lineTo(c.x() + 4, c.y() - 2);
            p.drawPath(path);
            break;
        }
        case Menu:
            for (int dy : {-5, 0, 5})
                p.drawLine(QPointF(c.x() - 7, c.y() + dy), QPointF(c.x() + 7, c.y() + dy));
            break;
        case Minimize:
            p.drawLine(QPointF(c.x() - 5, c.y()), QPointF(c.x() + 5, c.y()));
            break;
        case Maximize:
            p.drawRect(QRectF(c.x() - 5, c.y() - 5, 10, 10));
            break;
        case Restore:
            p.drawRect(QRectF(c.x() - 5, c.y() - 3, 8, 8));
            p.drawPolyline(QPolygonF({QPointF(c.x() - 3, c.y() - 5), QPointF(c.x() + 5, c.y() - 5),
                                      QPointF(c.x() + 5, c.y() + 3)}));
            break;
        case Close:
            p.drawLine(QPointF(c.x() - 5, c.y() - 5), QPointF(c.x() + 5, c.y() + 5));
            p.drawLine(QPointF(c.x() + 5, c.y() - 5), QPointF(c.x() - 5, c.y() + 5));
            break;
    }
}

// ── TabView ───────────────────────────────────────────────────────────────

namespace {
const int kSegPad = 4;       // inside the segment group
const int kSegHeight = 40;   // the group
const int kSegMin = 96, kSegMax = 230;
const int kRowHeight = 50;   // a session row
}

TabView::TabView(Style style, QWidget *parent)
    : QWidget(parent), m_style(style)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    if (style == Segments)
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    else
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
}

void TabView::setTabs(const QList<TabInfo> &tabs, int current)
{
    if (tabs == m_tabs && current == m_current)
        return;
    const bool countChanged = tabs.size() != m_tabs.size();
    m_tabs = tabs;
    m_current = current;
    if (countChanged || m_style == Segments)
        updateGeometry();
    update();
}

static QFont tabFont(bool bold, qreal pt)
{
    QFont f(QStringLiteral("Inter"));
    f.setPointSizeF(pt);
    f.setWeight(bold ? QFont::DemiBold : QFont::Medium);
    return f;
}

QSize TabView::sizeHint() const
{
    if (m_style == Sessions)
        return QSize(200, qMax(1, (int)m_tabs.size()) * (kRowHeight + 2));
    const QFontMetrics fm(tabFont(true, 9.5));
    int w = kSegPad * 2;
    for (const TabInfo &t : m_tabs)
    {
        int tw = fm.horizontalAdvance(t.title) + 28;
        if (!t.badge.isEmpty())
            tw += pillWidth(t.badge) + 8;
        w += qBound(kSegMin, tw, kSegMax) + 4;
    }
    return QSize(w, kSegHeight);
}

QSize TabView::minimumSizeHint() const
{
    if (m_style == Sessions)
        return QSize(160, kRowHeight);
    return QSize(kSegPad * 2 + 60, kSegHeight);
}

QList<QRect> TabView::layoutRects() const
{
    QList<QRect> rects;
    if (m_style == Sessions)
    {
        for (int i = 0; i < m_tabs.size(); ++i)
            rects << QRect(0, i * (kRowHeight + 2), width(), kRowHeight);
        return rects;
    }
    const QFontMetrics fm(tabFont(true, 9.5));
    QList<int> widths;
    int total = 0;
    for (const TabInfo &t : m_tabs)
    {
        int tw = fm.horizontalAdvance(t.title) + 28;
        if (!t.badge.isEmpty())
            tw += pillWidth(t.badge) + 8;
        tw = qBound(kSegMin, tw, kSegMax);
        widths << tw;
        total += tw + 4;
    }
    const int avail = width() - kSegPad * 2;
    if (total > avail && !widths.isEmpty())
    {
        const int each = qMax(56, avail / (int)widths.size() - 4);
        for (int &w : widths)
            w = qMin(w, each);
    }
    int x = kSegPad;
    for (int w : widths)
    {
        rects << QRect(x, kSegPad, w, kSegHeight - kSegPad * 2);
        x += w + 4;
    }
    return rects;
}

QRect TabView::closeRect(const QRect &tab) const
{
    const int s = 18;
    return QRect(tab.right() - s - 8, tab.center().y() - s / 2 + 1, s, s);
}

int TabView::indexAt(const QPoint &pos) const
{
    const QList<QRect> rects = layoutRects();
    for (int i = 0; i < rects.size(); ++i)
        if (rects.at(i).contains(pos))
            return i;
    return -1;
}

void TabView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QList<QRect> rects = layoutRects();

    if (m_style == Segments)
    {
        // The group: frosted glass.
        QRectF group(0.5, 0.5, (rects.isEmpty() ? 60 : rects.last().right() + kSegPad) - 0.5, kSegHeight - 1);
        p.setPen(QPen(QColor(255, 255, 255, 22), 1));
        p.setBrush(QColor(255, 255, 255, 10));
        p.drawRoundedRect(group, 12, 12);
    }

    for (int i = 0; i < m_tabs.size() && i < rects.size(); ++i)
    {
        const TabInfo &t = m_tabs.at(i);
        const QRectF r = QRectF(rects.at(i)).adjusted(0.5, 0.5, -0.5, -0.5);
        const bool on = i == m_current;
        const bool hover = i == m_hover;

        if (on)
        {
            p.setPen(QPen(QColor(255, 255, 255, 30), 1));
            p.setBrush(QColor(255, 255, 255, 26));
            p.drawRoundedRect(r, 9, 9);
            // A highlight along the top edge: light on glass.
            p.setPen(QPen(QColor(255, 255, 255, 40), 1));
            p.drawLine(QPointF(r.left() + 9, r.top() + 0.5), QPointF(r.right() - 9, r.top() + 0.5));
        }
        else if (hover)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 12));
            p.drawRoundedRect(r, 9, 9);
        }

        QRect content = rects.at(i).adjusted(m_style == Segments ? 14 : 10, 0, -10, 0);
        if (hover && m_tabs.size() > 1)
            content.setRight(closeRect(rects.at(i)).left() - 4);

        if (m_style == Sessions)
        {
            // The mark box.
            const QRectF box(content.left(), rects.at(i).center().y() - 12, 24, 24);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, on ? 22 : 12));
            p.drawRoundedRect(box, 7, 7);
            QFont mono(QStringLiteral("Hack"));
            mono.setPixelSize(12);
            p.setFont(mono);
            p.setPen(on ? kText : QColor(0xbd, 0xbd, 0xc2));
            p.drawText(box, Qt::AlignCenter, t.glyph);
            content.setLeft(box.right() + 10);
        }

        // The badge, right-aligned in a session row, before the title in a segment.
        if (!t.badge.isEmpty())
        {
            const qreal bw = pillWidth(t.badge);
            if (m_style == Sessions && !(hover && m_tabs.size() > 1))
            {
                const QRectF b(content.right() - bw, rects.at(i).center().y() - 8, bw, 16);
                drawPill(p, b, t.badge, t.solid);
                content.setRight(b.left() - 6);
            }
            else if (m_style == Segments)
            {
                const QRectF b(content.left(), rects.at(i).center().y() - 8, bw, 16);
                drawPill(p, b, t.badge, t.solid);
                content.setLeft(b.right() + 8);
            }
        }

        const QColor ink = on ? kText : (hover ? QColor(0xd8, 0xd8, 0xdc) : kGrey);
        if (m_style == Segments)
        {
            p.setFont(tabFont(on, 9.5));
            p.setPen(ink);
            const QString text = p.fontMetrics().elidedText(t.title, Qt::ElideRight, content.width());
            p.drawText(content, Qt::AlignVCenter | Qt::AlignLeft, text);
        }
        else
        {
            QRect top = content, bottom = content;
            top.setBottom(rects.at(i).center().y() + 1);
            bottom.setTop(rects.at(i).center().y() + 3);
            p.setFont(tabFont(on, 9.5));
            p.setPen(ink);
            p.drawText(top, Qt::AlignBottom | Qt::AlignLeft,
                       p.fontMetrics().elidedText(t.title, Qt::ElideRight, top.width()));
            p.setFont(tabFont(false, 8));
            p.setPen(kDim);
            p.drawText(bottom, Qt::AlignTop | Qt::AlignLeft,
                       p.fontMetrics().elidedText(t.subtitle, Qt::ElideRight, bottom.width()));
        }

        if (hover && m_tabs.size() > 1)
        {
            const QRectF c = closeRect(rects.at(i));
            if (m_hoverClose)
            {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 36));
                p.drawRoundedRect(c, 5, 5);
            }
            QPen pen(m_hoverClose ? kText : kGrey, 1.3);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            const QPointF cc = c.center();
            p.drawLine(cc + QPointF(-3.5, -3.5), cc + QPointF(3.5, 3.5));
            p.drawLine(cc + QPointF(3.5, -3.5), cc + QPointF(-3.5, 3.5));
        }
    }
}

void TabView::mouseMoveEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().toPoint());
    const bool onClose = i >= 0 && m_tabs.size() > 1
        && closeRect(layoutRects().at(i)).contains(e->position().toPoint());
    if (i != m_hover || onClose != m_hoverClose)
    {
        m_hover = i;
        m_hoverClose = onClose;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void TabView::leaveEvent(QEvent *e)
{
    m_hover = -1;
    m_hoverClose = false;
    update();
    QWidget::leaveEvent(e);
}

void TabView::mousePressEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().toPoint());
    if (i < 0)
    {
        e->ignore(); // empty space: the window moves (Chrome)
        return;
    }
    if (e->button() == Qt::LeftButton)
    {
        if (m_hoverClose && m_tabs.size() > 1)
            emit closeRequested(i);
        else
            emit activated(i);
    }
}

void TabView::mouseReleaseEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().toPoint());
    if (i >= 0 && e->button() == Qt::MiddleButton)
        emit closeRequested(i);
}

void TabView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().toPoint());
    if (i >= 0 && e->button() == Qt::LeftButton)
        emit renameRequested(i);
    else
        e->ignore();
}

void TabView::contextMenuEvent(QContextMenuEvent *e)
{
    const int i = indexAt(e->pos());
    if (i < 0)
        return;
    QMenu menu;
    menu.addAction(tr("Rename tab"), this, [this, i] { emit renameRequested(i); });
    QAction *close = menu.addAction(tr("Close tab"), this, [this, i] { emit closeRequested(i); });
    close->setEnabled(m_tabs.size() > 1);
    menu.exec(e->globalPos());
}

void TabView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    update();
}

bool TabView::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        auto *he = static_cast<QHelpEvent *>(e);
        const int i = indexAt(he->pos());
        if (i >= 0 && !m_tabs.at(i).tooltip.isEmpty())
            QToolTip::showText(he->globalPos(), m_tabs.at(i).tooltip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

// ── Chrome ────────────────────────────────────────────────────────────────

namespace {

// The Maze mark: the app icon's folded prompt and cursor without the tile —
// the same drawing as maze/maze-terminal-symbolic.svg, in its 1024 grid.
QPixmap promptMark(int size)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    // The mark spans x 290..770, y 290..734: fit it, centred.
    const qreal scale = size / 500.0;
    p.translate(size / 2.0, size / 2.0);
    p.scale(scale, scale);
    p.translate(-530, -512);

    QLinearGradient lit(0, 290, 0, 512);
    lit.setColorAt(0, QColor(0xff, 0xff, 0xff));
    lit.setColorAt(1, QColor(0xd4, 0xd4, 0xda));
    p.setBrush(lit);
    p.drawPolygon(QPolygonF({QPointF(290, 290), QPointF(400, 290), QPointF(622, 512), QPointF(512, 512)}));

    QLinearGradient shade(290, 512, 622, 734);
    shade.setColorAt(0, QColor(0x8e, 0x8e, 0x96));
    shade.setColorAt(1, QColor(0x4a, 0x4a, 0x50));
    p.setBrush(shade);
    p.drawPolygon(QPolygonF({QPointF(512, 512), QPointF(622, 512), QPointF(400, 734), QPointF(290, 734)}));

    p.fillRect(QRectF(650, 680, 120, 54), kText);
    return pm;
}

QLabel *markLabel(QWidget *parent, int size)
{
    auto *mark = new QLabel(parent);
    mark->setPixmap(promptMark(size));
    mark->setFixedSize(size, size);
    mark->setProperty("mazeDrag", true);
    return mark;
}

QWidget *dragArea(QWidget *parent, const char *name)
{
    auto *w = new QWidget(parent);
    w->setObjectName(QLatin1String(name));
    w->setAttribute(Qt::WA_StyledBackground);
    w->setProperty("mazeDrag", true);
    return w;
}

} // namespace

QWidget *Chrome::buildWindowButtons(QList<QAbstractButton *> &maximizeButtons)
{
    auto *box = new QWidget;
    auto *l = new QHBoxLayout(box);
    l->setContentsMargins(4, 0, 0, 0);
    l->setSpacing(2);
    auto *minimize = new GlyphButton(GlyphButton::Minimize, box);
    auto *maximize = new GlyphButton(GlyphButton::Maximize, box);
    auto *close = new GlyphButton(GlyphButton::Close, box);
    connect(minimize, &QAbstractButton::clicked, m_window, &QWidget::showMinimized);
    connect(maximize, &QAbstractButton::clicked, this, &Chrome::toggleMaximized);
    connect(close, &QAbstractButton::clicked, m_window, &QWidget::close);
    l->addWidget(minimize);
    l->addWidget(maximize);
    l->addWidget(close);
    maximizeButtons << maximize;
    m_windowButtonSets << box;
    return box;
}

GlyphButton *Chrome::menuButton(QWidget *parent)
{
    auto *b = new GlyphButton(GlyphButton::Menu, parent);
    b->setToolTip(tr("Menu"));
    connect(b, &QAbstractButton::clicked, this, [this, b] {
        const QPoint at = b->mapToGlobal(QPoint(b->width(), b->height() + 4));
        m_appMenu->exec(at - QPoint(m_appMenu->sizeHint().width(), 0));
    });
    return b;
}

QLabel *Chrome::chip(QWidget *parent, const QString &name)
{
    auto *l = new QLabel(parent);
    l->setObjectName(name.isEmpty() ? QStringLiteral("MazeChip") : name);
    l->setTextFormat(Qt::RichText);
    l->setFixedHeight(26);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

void Chrome::newTab(const char *name)
{
    if (QAction *a = m_hooks.action(name))
        a->trigger();
}

Chrome::Chrome(QMainWindow *window, QWidget *terminals, const QList<QMenu *> &menus, Hooks hooks)
    : QObject(window), m_window(window), m_terminals(terminals), m_hooks(std::move(hooks))
{
    m_appMenu = new QMenu(window);
    for (QMenu *m : menus)
        m_appMenu->addMenu(m);

    m_root = new QWidget;
    m_root->setObjectName(QStringLiteral("MazeRoot"));
    m_root->setAttribute(Qt::WA_StyledBackground);
    m_root->setProperty("mazeDrag", true);
    auto *rootLayout = new QVBoxLayout(m_root);
    m_rootLayout = rootLayout;
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    // ── B: header, card, footer ───────────────────────────────────────────
    m_topHeader = dragArea(m_root, "MazeTopHeader");
    m_topHeader->setFixedHeight(58);
    {
        auto *l = new QHBoxLayout(m_topHeader);
        l->setContentsMargins(18, 0, 10, 0);
        l->setSpacing(10);
        l->addWidget(markLabel(m_topHeader, 18));
        l->addSpacing(4);
        m_segments = new TabView(TabView::Segments, m_topHeader);
        l->addWidget(m_segments, 0, Qt::AlignVCenter);
        auto *plus = new GlyphButton(GlyphButton::Plus, m_topHeader);
        plus->setFramed(true);
        plus->setFixedSize(36, 36);
        plus->setToolTip(tr("New tab"));
        connect(plus, &QAbstractButton::clicked, this, [this] { newTab("Add Tab"); });
        auto *more = new GlyphButton(GlyphButton::Chevron, m_topHeader);
        more->setFixedSize(26, 36);
        more->setToolTip(tr("New Tor tab or incognito tab"));
        connect(more, &QAbstractButton::clicked, this, [this, more] {
            QMenu menu;
            for (const char *name : {"Add Tab", "Add Tor Tab", "Add Incognito Tab", "New Window"})
                if (QAction *a = m_hooks.action(name))
                    menu.addAction(a);
            menu.exec(more->mapToGlobal(QPoint(0, more->height() + 4)));
        });
        l->addWidget(plus);
        l->addWidget(more);
        l->addStretch(1);
        l->addWidget(menuButton(m_topHeader));
        l->addWidget(buildWindowButtons(m_maximizeButtons));
    }
    m_cardRow = new QWidget(m_root);
    {
        auto *l = new QHBoxLayout(m_cardRow);
        l->setContentsMargins(12, 0, 12, 0);
        m_cardRowLayout = l;
        auto *card = new QWidget(m_cardRow);
        m_card = card;
        card->setObjectName(QStringLiteral("MazeCard"));
        card->setAttribute(Qt::WA_StyledBackground);
        m_cardLayout = new QVBoxLayout(card);
        // Inset so the terminal's square corners stay inside the card's round ones.
        m_cardLayout->setContentsMargins(4, 4, 4, 4);
        l->addWidget(card);
    }
    m_topFooter = dragArea(m_root, "MazeTopFooter");
    m_topFooter->setFixedHeight(44);
    {
        auto *l = new QHBoxLayout(m_topFooter);
        l->setContentsMargins(14, 0, 14, 0);
        l->setSpacing(6);
        m_chipMode = chip(m_topFooter, QStringLiteral("MazeChipMode"));
        m_chipProgram = chip(m_topFooter);
        m_chipCwd = chip(m_topFooter);
        m_chipBranch = chip(m_topFooter);
        m_chipGuard = chip(m_topFooter);
        m_chipSize = chip(m_topFooter);
        l->addWidget(m_chipMode);
        l->addWidget(m_chipProgram);
        l->addWidget(m_chipCwd);
        l->addWidget(m_chipBranch);
        l->addStretch(1);
        l->addWidget(m_chipGuard);
        l->addWidget(m_chipSize);
    }

    // ── A: top bar, side panel, status ────────────────────────────────────
    m_sideTop = dragArea(m_root, "MazeSideTop");
    m_sideTop->setFixedHeight(42);
    {
        auto *l = new QHBoxLayout(m_sideTop);
        l->setContentsMargins(18, 0, 6, 0);
        m_crumb = new QLabel(m_sideTop);
        m_crumb->setObjectName(QStringLiteral("MazeCrumb"));
        m_crumb->setTextFormat(Qt::RichText);
        m_crumb->setProperty("mazeDrag", true);
        l->addWidget(m_crumb);
        l->addStretch(1);
        l->addWidget(menuButton(m_sideTop));
        l->addWidget(buildWindowButtons(m_maximizeButtons));
    }
    m_sideMiddle = new QWidget(m_root);
    {
        auto *l = new QHBoxLayout(m_sideMiddle);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(0);
        auto *side = dragArea(m_sideMiddle, "MazeSide");
        m_side = side;
        side->setFixedWidth(236);
        auto *sl = new QVBoxLayout(side);
        sl->setContentsMargins(10, 16, 10, 12);
        sl->setSpacing(6);
        auto *brand = new QHBoxLayout;
        brand->setContentsMargins(8, 0, 0, 10);
        brand->setSpacing(10);
        brand->addWidget(markLabel(side, 18));
        auto *word = new QLabel(QStringLiteral("MAZE TERMINAL"), side);
        word->setObjectName(QStringLiteral("MazeBrand"));
        word->setProperty("mazeDrag", true);
        brand->addWidget(word);
        brand->addStretch(1);
        sl->addLayout(brand);
        auto *section = new QLabel(tr("SESSIONS"), side);
        section->setObjectName(QStringLiteral("MazeSection"));
        section->setProperty("mazeDrag", true);
        sl->addWidget(section);
        auto *area = new QScrollArea(side);
        area->setObjectName(QStringLiteral("MazeSessionsArea"));
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_sessions = new TabView(TabView::Sessions);
        area->setWidget(m_sessions);
        sl->addWidget(area, 1);
        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(6);
        auto *bNew = new QPushButton(tr("+ New tab"), side);
        bNew->setObjectName(QStringLiteral("MazeSidePrimary"));
        auto *bTor = new QPushButton(tr("Tor"), side);
        bTor->setObjectName(QStringLiteral("MazeSideButton"));
        auto *bPriv = new QPushButton(tr("Private"), side);
        bPriv->setObjectName(QStringLiteral("MazeSideButton"));
        for (QPushButton *b : {bNew, bTor, bPriv})
        {
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
        }
        connect(bNew, &QPushButton::clicked, this, [this] { newTab("Add Tab"); });
        connect(bTor, &QPushButton::clicked, this, [this] { newTab("Add Tor Tab"); });
        connect(bPriv, &QPushButton::clicked, this, [this] { newTab("Add Incognito Tab"); });
        buttons->addWidget(bNew, 3);
        buttons->addWidget(bTor, 2);
        buttons->addWidget(bPriv, 2);
        sl->addLayout(buttons);
        l->addWidget(side);
        auto *termBox = new QWidget(m_sideMiddle);
        m_sideTermLayout = new QVBoxLayout(termBox);
        m_sideTermLayout->setContentsMargins(0, 0, 0, 0);
        l->addWidget(termBox, 1);
    }
    m_sideStatus = dragArea(m_root, "MazeSideStatus");
    m_sideStatus->setFixedHeight(30);
    {
        auto *l = new QHBoxLayout(m_sideStatus);
        l->setContentsMargins(0, 0, 0, 0);
        m_sideStatusLeft = new QLabel(m_sideStatus);
        m_sideStatusLeft->setObjectName(QStringLiteral("MazeStatusText"));
        m_sideStatusRight = new QLabel(m_sideStatus);
        m_sideStatusRight->setObjectName(QStringLiteral("MazeStatusText"));
        l->addWidget(m_sideStatusLeft, 1);
        l->addWidget(m_sideStatusRight);
    }

    rootLayout->addWidget(m_topHeader);
    rootLayout->addWidget(m_sideTop);
    rootLayout->addWidget(m_cardRow, 1);
    rootLayout->addWidget(m_sideMiddle, 1);
    rootLayout->addWidget(m_topFooter);
    rootLayout->addWidget(m_sideStatus);

    for (TabView *v : {m_segments, m_sessions})
    {
        connect(v, &TabView::activated, this, [this](int i) { m_hooks.activate(i); refresh(); });
        connect(v, &TabView::closeRequested, this, [this](int i) { m_hooks.close(i); refresh(); });
        connect(v, &TabView::renameRequested, this, [this](int i) { m_hooks.rename(i); refresh(); });
    }

    // The old central widget held the terminals; take them out before it goes.
    m_terminals->setParent(nullptr);
    m_window->setCentralWidget(m_root);

    auto *timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &Chrome::refresh);
    timer->start();

    m_window->installEventFilter(this);
    qApp->installEventFilter(this);
    setLayoutStyle(TopTabs);
}

void Chrome::setLayoutStyle(Layout layout)
{
    m_layout = layout;
    const bool top = layout == TopTabs;
    if (top)
        m_cardLayout->addWidget(m_terminals);
    else
        m_sideTermLayout->addWidget(m_terminals);
    m_terminals->show();
    applyVisibility();
    m_lastCurrent = -2;
    refresh();
}

// Which parts show: the chosen layout's, and in full screen only the
// terminal, edge to edge.
void Chrome::applyVisibility()
{
    const bool top = m_layout == TopTabs;
    const bool full = m_window->isFullScreen();
    m_topHeader->setVisible(top && !full);
    m_cardRow->setVisible(top);
    m_topFooter->setVisible(top && !full);
    m_sideTop->setVisible(!top && !full);
    m_sideMiddle->setVisible(!top);
    m_side->setVisible(!full);
    m_sideStatus->setVisible(!top && !full);

    m_cardRowLayout->setContentsMargins(full ? 0 : 12, 0, full ? 0 : 12, 0);
    m_cardLayout->setContentsMargins(full ? 0 : 4, full ? 0 : 4, full ? 0 : 4, full ? 0 : 4);
    if (m_card->property("flat").toBool() != full)
    {
        m_card->setProperty("flat", full);
        m_card->style()->unpolish(m_card);
        m_card->style()->polish(m_card);
    }
}

void Chrome::setGlass(bool glass)
{
    m_glass = glass;
    m_root->setProperty("glass", glass);
    m_root->style()->unpolish(m_root);
    m_root->style()->polish(m_root);
    updateBlur();
}

void Chrome::setFrameless(bool frameless)
{
    m_frameless = frameless;
    for (QWidget *w : std::as_const(m_windowButtonSets))
        w->setVisible(frameless);
    syncWindowState();
}

void Chrome::refresh()
{
    const QList<TabInfo> tabs = m_hooks.tabs();
    const int current = m_hooks.current();
    if (tabs != m_lastTabs || current != m_lastCurrent)
    {
        m_lastTabs = tabs;
        m_lastCurrent = current;
        m_segments->setTabs(tabs, current);
        m_sessions->setTabs(tabs, current);
    }

    const StatusInfo s = m_hooks.status();
    auto set = [](QLabel *l, const QString &text) {
        l->setVisible(!text.isEmpty());
        if (l->text() != text)
            l->setText(text);
    };
    if (m_layout == TopTabs)
    {
        set(m_chipMode, s.mode);
        set(m_chipProgram, s.program.isEmpty() ? QString()
            : QStringLiteral("<span style='color:#f2f2f4'>●</span>&nbsp; <b style='color:#f2f2f4'>%1</b>").arg(s.program.toHtmlEscaped()));
        set(m_chipCwd, s.cwd.toHtmlEscaped());
        set(m_chipBranch, s.branch.toHtmlEscaped());
        set(m_chipGuard, s.pasteGuard ? tr("Paste guard on") : tr("Paste guard off"));
        set(m_chipSize, s.size);
    }
    else
    {
        const QString folder = current >= 0 && current < tabs.size() ? tabs.at(current).title : QString();
        set(m_crumb, QStringLiteral("%1 <span style='color:#3a3a40'>/</span> <b style='color:#f2f2f4'>%2</b>")
                         .arg(folder.toHtmlEscaped(), s.program.toHtmlEscaped()));
        QString left = s.cwd.toHtmlEscaped();
        if (!s.mode.isEmpty())
            left = QStringLiteral("<b style='color:#f2f2f4'>%1</b> &nbsp;·&nbsp; %2").arg(s.mode, left);
        set(m_sideStatusLeft, left);
        QStringList right;
        if (!s.program.isEmpty()) right << s.program;
        if (!s.branch.isEmpty()) right << s.branch;
        right << s.size;
        set(m_sideStatusRight, right.join(QStringLiteral("  ·  ")));
    }
}

void Chrome::toggleMaximized()
{
    if (m_window->isMaximized())
        m_window->showNormal();
    else
        m_window->showMaximized();
}

void Chrome::syncWindowState()
{
    // Maximised, full screen or with a system frame, the window has square
    // corners and no edge, so it meets the screen edges exactly.
    const bool max = m_window->isMaximized() || m_window->isFullScreen() || !m_frameless;
    m_rootLayout->setContentsMargins(max ? 0 : 1, max ? 0 : 1, max ? 0 : 1, max ? 0 : 1);
    applyVisibility();
    for (QAbstractButton *b : std::as_const(m_maximizeButtons))
    {
        static_cast<GlyphButton *>(b)->setGlyph(m_window->isMaximized() ? GlyphButton::Restore : GlyphButton::Maximize);
        b->setToolTip(m_window->isMaximized() ? tr("Restore") : tr("Maximize"));
    }
    if (m_root->property("maximized").toBool() != max)
    {
        m_root->setProperty("maximized", max);
        m_root->style()->unpolish(m_root);
        m_root->style()->polish(m_root);
    }
    updateBlur();
}

void Chrome::updateBlur()
{
#ifdef HAVE_KWINDOWSYSTEM
    QWindow *h = m_window->windowHandle();
    if (!h)
        return;
    if (!m_glass)
    {
        KWindowEffects::enableBlurBehind(h, false);
        return;
    }
    const bool square = m_root->property("maximized").toBool();
    QPainterPath path;
    path.addRoundedRect(QRectF(m_window->rect()), square ? 0 : kRadius, square ? 0 : kRadius);
    KWindowEffects::enableBlurBehind(h, true, QRegion(path.toFillPolygon().toPolygon()));
#endif
}

Qt::Edges Chrome::edgesAt(const QPoint &globalPos) const
{
    Qt::Edges edges;
    if (!m_frameless || m_window->isMaximized() || m_window->isFullScreen())
        return edges;
    const QPoint p = m_window->mapFromGlobal(globalPos);
    const QRect r = m_window->rect();
    if (!r.contains(p))
        return edges;
    if (p.x() < kEdge) edges |= Qt::LeftEdge;
    if (p.x() >= r.width() - kEdge) edges |= Qt::RightEdge;
    if (p.y() < kEdge) edges |= Qt::TopEdge;
    if (p.y() >= r.height() - kEdge) edges |= Qt::BottomEdge;
    return edges;
}

static Qt::CursorShape cursorFor(Qt::Edges e)
{
    if (e == (Qt::LeftEdge | Qt::TopEdge) || e == (Qt::RightEdge | Qt::BottomEdge))
        return Qt::SizeFDiagCursor;
    if (e == (Qt::RightEdge | Qt::TopEdge) || e == (Qt::LeftEdge | Qt::BottomEdge))
        return Qt::SizeBDiagCursor;
    if (e & (Qt::LeftEdge | Qt::RightEdge))
        return Qt::SizeHorCursor;
    return Qt::SizeVerCursor;
}

bool Chrome::eventFilter(QObject *obj, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (obj == m_window)
    {
        if (type == QEvent::WindowStateChange)
            syncWindowState();
        else if (type == QEvent::Resize || type == QEvent::Show)
            QTimer::singleShot(0, this, &Chrome::updateBlur);
        return false;
    }

    if (type != QEvent::MouseMove && type != QEvent::MouseButtonPress
        && type != QEvent::MouseButtonDblClick)
        return false;

    auto *widget = qobject_cast<QWidget *>(obj);
    if (!widget || widget->window() != m_window)
        return false;

    // Window edges: resize. Checked for every widget of this window, since
    // the terminal covers nearly all of it.
    auto *me = static_cast<QMouseEvent *>(event);
    const Qt::Edges edges = edgesAt(me->globalPosition().toPoint());
    if (type == QEvent::MouseMove && me->buttons() == Qt::NoButton)
    {
        if (edges && !m_cursorSet)
        {
            QGuiApplication::setOverrideCursor(cursorFor(edges));
            m_cursorSet = true;
        }
        else if (edges && m_cursorSet)
        {
            QGuiApplication::changeOverrideCursor(cursorFor(edges));
        }
        else if (!edges && m_cursorSet)
        {
            QGuiApplication::restoreOverrideCursor();
            m_cursorSet = false;
        }
        return false;
    }
    if (type == QEvent::MouseButtonPress && me->button() == Qt::LeftButton && edges)
    {
        if (QWindow *h = m_window->windowHandle())
            h->startSystemResize(edges);
        return true;
    }

    // Empty header, footer and side panel space: move, double click maximises.
    if (!m_frameless || me->button() != Qt::LeftButton)
        return false;
    bool drag = widget->property("mazeDrag").toBool();
    if (auto *view = qobject_cast<TabView *>(widget))
    {
        // A tab view's own empty space (right of the last segment).
        Q_UNUSED(view);
        drag = false;
    }
    if (!drag)
        return false;
    if (type == QEvent::MouseButtonDblClick)
        toggleMaximized();
    else if (QWindow *h = m_window->windowHandle())
        h->startSystemMove();
    return true;
}

} // namespace Maze
