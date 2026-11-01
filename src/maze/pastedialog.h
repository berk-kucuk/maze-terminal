/***************************************************************************
 *   Maze Terminal — the paste guard's question.                           *
 *   Copyright (C) 2026 Berk Küçük <berkkucukk@proton.me>                  *
 *   GPL-2.0-or-later, like the rest of this program.                      *
 ***************************************************************************/
#ifndef MAZE_PASTEDIALOG_H
#define MAZE_PASTEDIALOG_H

#include <QDialog>

#include "pasteguard.h"

namespace Maze {

class PasteDialog : public QDialog
{
    Q_OBJECT
public:
    enum Choice { Cancel, AsIs, Cleaned };

    // Ask before a risky paste. Returns what the user chose; Cancel unless
    // they chose otherwise — the safe answer is the default one.
    static Choice ask(const QString &text, const PasteVerdict &verdict, QWidget *parent);

private:
    PasteDialog(const QString &text, const PasteVerdict &verdict, QWidget *parent);
    Choice m_choice = Cancel;
};

} // namespace Maze

#endif
