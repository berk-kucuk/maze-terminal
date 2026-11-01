/***************************************************************************
 *   Maze Terminal — paste guard.                                          *
 *   Copyright (C) 2026 Berk Küçük <berkkucukk@proton.me>                  *
 *   GPL-2.0-or-later, like the rest of this program.                      *
 ***************************************************************************/
#ifndef MAZE_PASTEGUARD_H
#define MAZE_PASTEGUARD_H

#include <QString>
#include <QStringList>

/*
 * What a paste would really do, decided before it reaches the shell.
 *
 * A web page can put text on the clipboard that is not what it shows: a
 * zero-width character inside a command, a right-to-left override that makes
 * "rm -rf ~/ #lmth.txt" read as something harmless, or an escape sequence
 * that ends bracketed paste early — after which the rest of the paste runs
 * as typed commands, newline and all, the moment it lands.
 *
 *   Danger  control characters, escape sequences, invisible and direction-
 *           changing characters: what runs is not what you see.
 *   Caution visible but worth a second look: download-and-run, sudo, a
 *           recursive delete of / or ~.
 *   Clean   pasted at once, no dialog — the common case must stay fast.
 */
namespace Maze {

struct PasteVerdict {
    enum Level { Clean = 0, Caution = 1, Danger = 2 };
    Level level = Clean;
    QStringList reasons;    // translated, one per finding
    int lines = 1;          // lines of text, for the dialog
};

class PasteGuard {
public:
    static PasteVerdict inspect(const QString &text);

    // The text with every character that is not what it looks like made
    // visible: ␛ for ESC, ⟦U+200B⟧ for invisible ones, ⏎ before line breaks.
    static QString reveal(const QString &text);

    // The text with control, escape, invisible and direction characters
    // removed and trailing newlines stripped, so nothing runs on its own.
    static QString clean(const QString &text);

    static bool isInvisible(char32_t c);
};

} // namespace Maze

#endif
