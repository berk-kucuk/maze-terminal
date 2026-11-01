/***************************************************************************
 *   Maze Terminal — the window around the terminal                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef MAZE_CHROME_H
#define MAZE_CHROME_H

#include <QAbstractButton>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QWidget>
#include <functional>

class QApplication;
class QBoxLayout;
class QLabel;
class QMainWindow;
class QMenu;
class QPushButton;
class QScrollArea;

namespace Maze {

// Maze's OLED look for every widget the app draws: true black, greys from
// the rest of the suite, white as the one contrast. No hue — the terminal's
// own colour scheme is the only colour on screen.
void applyTheme(QApplication &app);

// A small monochrome pill for a tab: outlined for Tor and private, solid
// white for root, so the dangerous one is the loudest.
QIcon badge(const QString &text, bool solid);
QSize badgeSize();

// What the window shows about one tab, and about the current terminal.
struct TabInfo
{
    QString title;     // folder or running program
    QString subtitle;  // side layout only: "zsh · main*", "history off", …
    QString glyph;     // side layout only: a one-character mark
    QString badge;     // TOR / PRIVATE / ROOT, or empty
    bool solid = false;
    QString tooltip;
    bool operator==(const TabInfo &o) const
    {
        return title == o.title && subtitle == o.subtitle && glyph == o.glyph && badge == o.badge
            && solid == o.solid && tooltip == o.tooltip;
    }
};

struct StatusInfo
{
    QString program;   // foreground program, e.g. zsh
    QString cwd;       // with ~ for home
    QString branch;    // git branch, or empty
    QString mode;      // TOR / PRIVATE / ROOT, or empty
    QString size;      // 145×44
    bool pasteGuard = true;
};

// Buttons drawn as thin glyphs, like the window buttons of the Maze apps.
class GlyphButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum Glyph { Plus, Chevron, Menu, Minimize, Maximize, Restore, Close };
    GlyphButton(Glyph glyph, QWidget *parent = nullptr);
    void setGlyph(Glyph glyph);
    void setFramed(bool framed) { m_framed = framed; update(); }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    Glyph m_glyph;
    bool m_hover = false;
    bool m_framed = false;
};

// The tabs, drawn by Maze Terminal itself: a segmented row of pills (top
// layout) or a list of sessions (side layout). The real QTabWidget keeps
// doing the work; this only shows it and forwards clicks.
class TabView : public QWidget
{
    Q_OBJECT
public:
    enum Style { Segments, Sessions };
    TabView(Style style, QWidget *parent = nullptr);
    void setTabs(const QList<TabInfo> &tabs, int current);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void activated(int index);
    void closeRequested(int index);
    void renameRequested(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool event(QEvent *) override;

private:
    QList<QRect> layoutRects() const;
    QRect closeRect(const QRect &tab) const;
    int indexAt(const QPoint &pos) const;

    Style m_style;
    QList<TabInfo> m_tabs;
    int m_current = -1;
    int m_hover = -1;
    bool m_hoverClose = false;
};

// The window: header, the terminal in its frame, the footer; frameless,
// with its own window buttons, moved by its empty space and resized by its
// edges. Two layouts, chosen in Preferences → Window.
class Chrome : public QObject
{
    Q_OBJECT
public:
    enum Layout { TopTabs = 0, SideTabs = 1 };

    struct Hooks
    {
        std::function<QList<TabInfo>()> tabs;
        std::function<int()> current;
        std::function<void(int)> activate;
        std::function<void(int)> close;
        std::function<void(int)> rename;
        std::function<StatusInfo()> status;
        std::function<QAction *(const char *name)> action;
    };

    Chrome(QMainWindow *window, QWidget *terminals, const QList<QMenu *> &menus, Hooks hooks);

    void setLayoutStyle(Layout layout);
    void setGlass(bool glass);
    void setFrameless(bool frameless);

public slots:
    void refresh();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QWidget *buildWindowButtons(QList<QAbstractButton *> &maximizeButtons);
    GlyphButton *menuButton(QWidget *parent);
    QLabel *chip(QWidget *parent, const QString &name = QString());
    Qt::Edges edgesAt(const QPoint &globalPos) const;
    void toggleMaximized();
    void syncWindowState();
    void updateBlur();
    void newTab(const char *action);
    void applyVisibility();

    QMainWindow *m_window;
    QWidget *m_terminals;
    Hooks m_hooks;
    QMenu *m_appMenu;

    QWidget *m_root;
    QBoxLayout *m_rootLayout;
    QBoxLayout *m_cardRowLayout;
    QWidget *m_card;
    QWidget *m_side;
    // Top layout (B)
    QWidget *m_topHeader;
    TabView *m_segments;
    QWidget *m_cardRow;
    QBoxLayout *m_cardLayout;
    QWidget *m_topFooter;
    QLabel *m_chipProgram, *m_chipCwd, *m_chipBranch, *m_chipMode, *m_chipGuard, *m_chipSize;
    // Side layout (A)
    QWidget *m_sideTop;
    QLabel *m_crumb;
    QWidget *m_sideMiddle;
    QBoxLayout *m_sideTermLayout;
    TabView *m_sessions;
    QWidget *m_sideStatus;
    QLabel *m_sideStatusLeft, *m_sideStatusRight;

    QList<QWidget *> m_windowButtonSets;
    QList<QAbstractButton *> m_maximizeButtons;

    Layout m_layout = TopTabs;
    bool m_glass = true;
    bool m_frameless = false;
    bool m_cursorSet = false;
    QList<TabInfo> m_lastTabs;
    int m_lastCurrent = -2;
};

} // namespace Maze

#endif
