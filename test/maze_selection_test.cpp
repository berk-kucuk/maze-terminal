// Maze Terminal: dragging a selection past the top edge must scroll into the
// history AND keep where the drag started. Dragging up used to keep only
// what was on screen: the start point, once scrolled below the window, was
// clamped to the last visible line.
#include <QtTest>
#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>

#include "qtermwidget.h"
#include "TerminalDisplay.h"

using Konsole::TerminalDisplay;

class SelectionTest : public QObject
{
    Q_OBJECT

    QTermWidget *m_term = nullptr;
    TerminalDisplay *m_display = nullptr;

    void send(QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
    {
        const QPointF p(pos);
        QMouseEvent ev(type, p, m_display->mapToGlobal(p), Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(m_display, &ev);
    }

    QString dragSelect(QPoint from, QPoint to, int steps)
    {
        send(QEvent::MouseButtonPress, from, Qt::LeftButton);
        for (int i = 1; i <= steps; ++i) {
            // Every move beyond the edge scrolls a little further.
            send(QEvent::MouseMove, to + QPoint(0, i % 2), Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        send(QEvent::MouseButtonRelease, to, Qt::NoButton);
        return m_term->selectedText(true);
    }

    QString wheelSelect(QPoint from, int notches, QPoint end)
    {
        // Press, roll the wheel with the button held, then move a little and let go.
        send(QEvent::MouseButtonPress, from, Qt::LeftButton);
        send(QEvent::MouseMove, from + QPoint(4, 0), Qt::LeftButton);
        const QPointF p(from);
        for (int i = 0; i < qAbs(notches); ++i) {
            QWheelEvent w(p, m_display->mapToGlobal(p), QPoint(), QPoint(0, notches > 0 ? 120 : -120),
                          Qt::LeftButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(m_display, &w);
            QCoreApplication::processEvents();
        }
        send(QEvent::MouseMove, end, Qt::LeftButton);
        send(QEvent::MouseMove, end + QPoint(1, 0), Qt::LeftButton);
        send(QEvent::MouseButtonRelease, end, Qt::NoButton);
        return m_term->selectedText(true);
    }

    // The line a drag from `start` ends on when nothing scrolls: what any
    // longer selection from the same start must still end with.
    QString anchorLine(QPoint start)
    {
        const int top = m_display->contentsRect().top() + m_display->fontHeight() / 2 + 12;
        send(QEvent::MouseButtonPress, start, Qt::LeftButton);
        send(QEvent::MouseMove, QPoint(5, top), Qt::LeftButton);
        send(QEvent::MouseMove, QPoint(6, top), Qt::LeftButton);
        send(QEvent::MouseButtonRelease, QPoint(6, top), Qt::NoButton);
        return m_term->selectedText(true).trimmed().section(QLatin1Char('\n'), -1);
    }

private slots:
    void initTestCase()
    {
        m_term = new QTermWidget(0);
        m_term->setHistorySize(10000);
        m_term->setMargin(12);             // Maze default (TerminalMargin=12)
        m_term->setShellProgram(QStringLiteral("/bin/sh"));
        m_term->setArgs({QStringLiteral("-c"), QStringLiteral("seq 1 400; exec sleep 30")});
        m_term->resize(640, 400);
        m_term->show();
        m_term->startShellProgram();
        m_display = m_term->findChild<TerminalDisplay *>();
        QVERIFY(m_display);
        // Wait until all 400 lines arrived (the history then holds them).
        QTRY_VERIFY_WITH_TIMEOUT(m_term->historyLinesCount() > 300, 10000);
        QTest::qWait(200);
    }

    void dragUpwardKeepsTheStart()
    {
        auto *bar = m_display->findChild<QScrollBar *>();
        QVERIFY(bar);
        QCOMPARE(bar->value(), bar->maximum());           // at the bottom
        const int lineH = m_display->fontHeight();
        // "400" is the last line printed; the cursor waits on the empty line
        // under it, so the drag starts one line above the cursor.
        const int cursorRow = m_term->screenLinesCount() - 1;   // output filled the screen
        const QPoint start(m_display->width() / 2, m_display->contentsRect().top() + 1
                                                       + cursorRow * lineH + lineH / 2);
        const QString expectedEnd = anchorLine(start);
        const QString text = dragSelect(start, QPoint(5, -lineH), 60);
        qInfo("selected %lld lines, first '%s', last '%s'", (long long)text.count(QLatin1Char('\n')),
              qPrintable(text.section(QLatin1Char('\n'), 0, 0)),
              qPrintable(text.trimmed().section(QLatin1Char('\n'), -1)));

        QVERIFY2(bar->value() < bar->maximum() - m_term->screenLinesCount(),
                 "the drag did not scroll into the history");
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY2(lines.size() > m_term->screenLinesCount(),
                 qPrintable(QStringLiteral("only %1 lines selected — no more than one screen")
                                .arg(lines.size())));
        // The selection must still reach the line the drag started on.
        QCOMPARE(text.trimmed().section(QLatin1Char('\n'), -1), expectedEnd);
    }

    void dragDownwardStillWorks()
    {
        auto *bar = m_display->findChild<QScrollBar *>();
        bar->setValue(0);                                  // top of the history
        QCoreApplication::processEvents();
        const int lineH = m_display->fontHeight();
        const QString text = dragSelect(QPoint(5, lineH / 2),
                                        QPoint(5, m_display->height() + lineH), 60);
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY(lines.size() > m_term->screenLinesCount());
        QVERIFY(text.startsWith(QLatin1String("1\n")));
    }

    void wheelUpWhileSelecting()
    {
        auto *bar = m_display->findChild<QScrollBar *>();
        bar->setValue(bar->maximum());
        QCoreApplication::processEvents();
        const int lineH = m_display->fontHeight();
        const int top = m_display->contentsRect().top() + 1;
        const int lastRow = m_term->screenLinesCount() - 1;
        const QPoint start(m_display->width() / 2, top + lastRow * lineH + lineH / 2);
        const QString expectedEnd = anchorLine(start);
        const QString text = wheelSelect(start, 10, QPoint(5, top + lineH / 2));
        qInfo("wheel up: scrolled to %d/%d, selected %lld lines, first '%s', last '%s'",
              bar->value(), bar->maximum(), (long long)text.count(QLatin1Char('\n')),
              qPrintable(text.section(QLatin1Char('\n'), 0, 0)),
              qPrintable(text.trimmed().section(QLatin1Char('\n'), -1)));
        QVERIFY2(text.count(QLatin1Char('\n')) > m_term->screenLinesCount(),
                 "selection did not grow into the history while the wheel scrolled up");
        // The start must survive being scrolled off the bottom of the window.
        QCOMPARE(text.trimmed().section(QLatin1Char('\n'), -1), expectedEnd);
    }

    void wheelDownWhileSelecting()
    {
        auto *bar = m_display->findChild<QScrollBar *>();
        bar->setValue(0);
        QCoreApplication::processEvents();
        const int lineH = m_display->fontHeight();
        const int top = m_display->contentsRect().top() + 1;
        const int lastRow = m_term->screenLinesCount() - 1;
        const QString text = wheelSelect(QPoint(5, top + lineH / 2), -10,
                                         QPoint(m_display->width() / 2, top + lastRow * lineH + lineH / 2));
        qInfo("wheel down: scrolled to %d/%d, selected %lld lines, first '%s', last '%s'",
              bar->value(), bar->maximum(), (long long)text.count(QLatin1Char('\n')),
              qPrintable(text.section(QLatin1Char('\n'), 0, 0)),
              qPrintable(text.trimmed().section(QLatin1Char('\n'), -1)));
        QVERIFY(text.count(QLatin1Char('\n')) > m_term->screenLinesCount());
    }

    void cleanupTestCase() { delete m_term; }
};

QTEST_MAIN(SelectionTest)
#include "maze_selection_test.moc"
