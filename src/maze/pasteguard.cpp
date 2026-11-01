/***************************************************************************
 *   Maze Terminal — paste guard.                                          *
 *   Copyright (C) 2026 Berk Küçük <berkkucukk@proton.me>                  *
 *   GPL-2.0-or-later, like the rest of this program.                      *
 ***************************************************************************/
#include "pasteguard.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace Maze {

static QString tr(const char *s)
{
    return QCoreApplication::translate("Maze::PasteGuard", s);
}

bool PasteGuard::isInvisible(char32_t c)
{
    // Characters that change what a line means without being seen: zero-width
    // spaces and joiners, bidirectional controls (the "Trojan Source" set),
    // the soft hyphen, word joiners, the BOM, tag characters, and the
    // no-break space, which a shell does not treat as a space.
    if (c == 0x00A0 || c == 0x00AD || c == 0x034F || c == 0x061C || c == 0x115F
        || c == 0x1160 || c == 0x17B4 || c == 0x17B5 || c == 0x180E || c == 0x3164
        || c == 0xFEFF || c == 0xFFA0)
        return true;
    if ((c >= 0x200B && c <= 0x200F) || (c >= 0x202A && c <= 0x202F)
        || (c >= 0x2060 && c <= 0x206F) || (c >= 0xFE00 && c <= 0xFE0F)
        || (c >= 0xE0000 && c <= 0xE007F))
        return true;
    return false;
}

static bool isControl(char32_t c)
{
    // C0 except tab and line breaks, DEL, and C1. ESC (0x1B) is the one that
    // matters most: it can end bracketed paste early.
    if (c == '\t' || c == '\n' || c == '\r')
        return false;
    return c < 0x20 || c == 0x7F || (c >= 0x80 && c <= 0x9F);
}

PasteVerdict PasteGuard::inspect(const QString &text)
{
    PasteVerdict v;
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QString body = normalized;
    while (body.endsWith(QLatin1Char('\n')))
        body.chop(1);
    v.lines = body.isEmpty() ? 1 : body.count(QLatin1Char('\n')) + 1;

    bool escape = false, control = false, invisible = false;
    for (char32_t c : text.toUcs4()) {
        if (c == 0x1B || c == 0x9B)
            escape = true;
        else if (isControl(c))
            control = true;
        else if (isInvisible(c))
            invisible = true;
    }
    if (escape)
        v.reasons << tr("It contains terminal escape sequences. They can end the paste early "
                        "and run the rest as commands, without you pressing Enter.");
    if (control)
        v.reasons << tr("It contains control characters you cannot see.");
    if (invisible)
        v.reasons << tr("It contains invisible or direction-changing characters: the command "
                        "that would run is not the one shown on the page.");
    if (escape || control || invisible)
        v.level = PasteVerdict::Danger;

    static const QRegularExpression pipeToShell(
        QStringLiteral(R"((curl|wget|fetch)\b[^\n|]*\|\s*(sudo\s+)?(ba|z|da|k|fi)?sh\b)"
                       R"(|\b(ba|z)?sh\s+<\(\s*(curl|wget))"
                       R"(|eval\s+"?\$\(\s*(curl|wget))"
                       R"(|base64\s+(-d|--decode)[^\n|]*\|\s*(sudo\s+)?(ba|z)?sh\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sudo(QStringLiteral(R"((^|[;&|\s])(sudo|doas|pkexec)\s)"));
    static const QRegularExpression wipe(
        QStringLiteral(R"(\brm\s+(-[a-zA-Z]*[rR][a-zA-Z]*\s+)+(--no-preserve-root\s+)?(/|~|\$HOME)(\s|/?\*|$))"));
    static const QRegularExpression ddDisk(QStringLiteral(R"(\b(dd|mkfs[.\w]*)\b[^\n]*/dev/(sd|nvme|vd|mmcblk))"));

    bool caution = false;
    if (pipeToShell.match(body).hasMatch()) {
        v.reasons << tr("It downloads something and runs it straight away, unseen.");
        caution = true;
    }
    if (sudo.match(body).hasMatch()) {
        v.reasons << tr("It runs a command as root.");
        caution = true;
    }
    if (wipe.match(body).hasMatch()) {
        v.reasons << tr("It deletes everything under / or your home folder.");
        caution = true;
    }
    if (ddDisk.match(body).hasMatch()) {
        v.reasons << tr("It writes directly to a disk.");
        caution = true;
    }
    if (caution && v.level < PasteVerdict::Caution)
        v.level = PasteVerdict::Caution;
    return v;
}

QString PasteGuard::reveal(const QString &text)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    QString out;
    for (char32_t c : normalized.toUcs4()) {
        if (c == 0x1B)
            out += QStringLiteral("␛");
        else if (c == '\r' || c == '\n')
            out += QStringLiteral("⏎\n");
        else if (c == '\t')
            out += QStringLiteral("⇥");
        else if (isControl(c) || isInvisible(c))
            out += QStringLiteral("⟦U+%1⟧").arg(
                QString::number(uint(c), 16).toUpper().rightJustified(4, QLatin1Char('0')));
        else
            out += QString::fromUcs4(&c, 1);
    }
    return out;
}

QString PasteGuard::clean(const QString &text)
{
    QString out;
    for (char32_t c : text.toUcs4()) {
        if (c == 0x1B || c == 0x9B || isControl(c))
            continue;
        if (isInvisible(c)) {
            if (c == 0x00A0)
                out += QLatin1Char(' ');      // a no-break space becomes a real one
            continue;
        }
        out += QString::fromUcs4(&c, 1);
    }
    out.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    while (out.endsWith(QLatin1Char('\n')) || out.endsWith(QLatin1Char('\r')))
        out.chop(1);
    return out;
}

} // namespace Maze
