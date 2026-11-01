/***************************************************************************
 *   Maze Terminal — the paste guard's question.                           *
 *   Copyright (C) 2026 Berk Küçük <berkkucukk@proton.me>                  *
 *   GPL-2.0-or-later, like the rest of this program.                      *
 ***************************************************************************/
#include "pastedialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Maze {

static const int kShownChars = 6000;

PasteDialog::PasteDialog(const QString &text, const PasteVerdict &verdict, QWidget *parent)
    : QDialog(parent)
{
    const bool danger = verdict.level == PasteVerdict::Danger;
    setWindowTitle(danger ? tr("This paste is not what it looks like")
                          : tr("Check this paste before it runs"));
    setMinimumWidth(620);

    auto *v = new QVBoxLayout(this);
    v->setSpacing(10);

    auto *head = new QLabel(danger
        ? tr("<b>Maze Terminal stopped this paste.</b> It was not sent to the shell.")
        : tr("<b>Maze Terminal is holding this paste.</b> Nothing has run yet."), this);
    head->setWordWrap(true);
    v->addWidget(head);

    QString reasons;
    for (const QString &r : verdict.reasons)
        reasons += QStringLiteral("<li>%1</li>").arg(r.toHtmlEscaped());
    auto *why = new QLabel(QStringLiteral("<ul style='margin-left:-20px'>%1</ul>").arg(reasons), this);
    why->setWordWrap(true);
    why->setTextFormat(Qt::RichText);
    v->addWidget(why);

    auto *what = new QLabel(danger
        ? tr("What would really be pasted — hidden characters shown as ␛ and ⟦U+…⟧:")
        : tr("What would be pasted (%n line(s)):", nullptr, verdict.lines), this);
    v->addWidget(what);

    auto *box = new QPlainTextEdit(this);
    box->setReadOnly(true);
    box->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    box->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QString shown = PasteGuard::reveal(text);
    if (shown.size() > kShownChars) {
        shown.truncate(kShownChars);
        shown += QStringLiteral("\n…");
    }
    box->setPlainText(shown);
    box->setMinimumHeight(140);
    v->addWidget(box, 1);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *cancel = buttons->addButton(tr("Don't paste"), QDialogButtonBox::RejectRole);
    if (danger) {
        QPushButton *cleaned = buttons->addButton(tr("Paste without hidden characters"),
                                                  QDialogButtonBox::AcceptRole);
        cleaned->setToolTip(tr("Removes escape sequences, control and invisible characters and "
                               "the trailing line break, so nothing runs on its own.\n"
                               "The clipboard is replaced by the cleaned text too."));
        connect(cleaned, &QPushButton::clicked, this, [this] { m_choice = Cleaned; accept(); });
    }
    QPushButton *asIs = buttons->addButton(danger ? tr("Paste exactly as it is") : tr("Paste"),
                                           danger ? QDialogButtonBox::DestructiveRole
                                                  : QDialogButtonBox::AcceptRole);
    connect(asIs, &QPushButton::clicked, this, [this] { m_choice = AsIs; accept(); });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    cancel->setDefault(true);
    cancel->setFocus();
    v->addWidget(buttons);
}

PasteDialog::Choice PasteDialog::ask(const QString &text, const PasteVerdict &verdict, QWidget *parent)
{
    PasteDialog d(text, verdict, parent);
    d.exec();
    return d.m_choice;
}

} // namespace Maze
