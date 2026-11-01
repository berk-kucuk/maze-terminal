

#include <QHash>
#include <QString>

#include "qterminalapp.h"
#include "terminalconfig.h"
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include "properties.h"
#include "termwidget.h"

TerminalConfig::TerminalConfig(const QString &wdir, const QStringList &shell)
    : m_workingDirectory(wdir)
    , m_shell(shell)
{
}

TerminalConfig::TerminalConfig() = default;

TerminalConfig::TerminalConfig(const TerminalConfig &cfg)
    : m_currentDirectory(cfg.m_currentDirectory),
      m_workingDirectory(cfg.m_workingDirectory),
      m_shell(cfg.m_shell),
      m_kind(cfg.m_kind) {}

QString TerminalConfig::getWorkingDirectory()
{
    if (!m_workingDirectory.isEmpty())
        return m_workingDirectory;
    if (Properties::Instance()->useCWD && !m_currentDirectory.isEmpty())
        return m_currentDirectory;
    return QTerminalApp::Instance()->getWorkingDirectory();
}

// Whether this entropy-shield-run has --log (Entropy Shield 4.3+, later builds).
static bool launcherHasLog()
{
    static int has = -1;
    if (has < 0)
    {
        QProcess p;
        p.start(QStringLiteral("entropy-shield-run"), {QStringLiteral("--help")});
        p.waitForFinished(5000);
        has = p.readAllStandardOutput().contains("--log") ? 1 : 0;
    }
    return has == 1;
}

static void writeFile(const QString &path, const QByteArray &text)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(text);
}

// Start-up files for a Tor tab's shell. Each runs the user's own file of the
// same name (as VS Code's shell integration does), then .zshrc / the bashrc
// prints how the Tor connection went: after the shell's own greeting —
// fastfetch and the like — instead of before it.
static QString torRcDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                        + QStringLiteral("/maze-terminal");
    QDir().mkpath(dir + QStringLiteral("/zsh"));
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    const QByteArray show =
        "if [[ -n \"$MAZE_TERMINAL_NOTE\" && -f \"$MAZE_TERMINAL_NOTE\" ]]; then\n"
        "  printf '\\e[2m%s\\e[0m\\n' \"$(<\"$MAZE_TERMINAL_NOTE\")\"\n"
        "  command rm -f -- \"$MAZE_TERMINAL_NOTE\"\n"
        "fi\n"
        "unset MAZE_TERMINAL_NOTE\n";

    writeFile(dir + QStringLiteral("/zsh/.zshenv"),
        "# Maze Terminal (Tor tab): run the user's own .zshenv.\n"
        "__maze_zdotdir=\"$ZDOTDIR\"\n"
        "ZDOTDIR=\"${MAZE_USER_ZDOTDIR:-$HOME}\"\n"
        "[[ -f \"$ZDOTDIR/.zshenv\" ]] && source \"$ZDOTDIR/.zshenv\"\n"
        "MAZE_USER_ZDOTDIR=\"$ZDOTDIR\"\n"
        "ZDOTDIR=\"$__maze_zdotdir\"\n");
    writeFile(dir + QStringLiteral("/zsh/.zprofile"),
        "# Maze Terminal (Tor tab): run the user's own .zprofile.\n"
        "ZDOTDIR=\"$MAZE_USER_ZDOTDIR\"\n"
        "[[ -f \"$ZDOTDIR/.zprofile\" ]] && source \"$ZDOTDIR/.zprofile\"\n"
        "ZDOTDIR=\"$__maze_zdotdir\"\n");
    writeFile(dir + QStringLiteral("/zsh/.zshrc"),
        "# Maze Terminal (Tor tab): run the user's own .zshrc, then show the Tor log.\n"
        "ZDOTDIR=\"$MAZE_USER_ZDOTDIR\"\n"
        "[[ -f \"$ZDOTDIR/.zshrc\" ]] && source \"$ZDOTDIR/.zshrc\"\n"
        + show +
        "[[ \"$ZDOTDIR\" == \"$HOME\" ]] && unset ZDOTDIR\n"
        "unset MAZE_USER_ZDOTDIR __maze_zdotdir\n");
    writeFile(dir + QStringLiteral("/bashrc"),
        "# Maze Terminal (Tor tab): run the user's own .bashrc, then show the Tor log.\n"
        "[[ -f ~/.bashrc ]] && source ~/.bashrc\n" + show);
    return dir;
}

QStringList TerminalConfig::getShell(QStringList *extraEnv)
{
    if (m_kind == MazeKind::Tor)
    {
        // The shell, or the command asked for, started inside Entropy
        // Shield's network namespace: it and everything it runs have no
        // network but Tor. If entropy-shield-run is missing the tab simply
        // fails to start — it never falls back to a normal shell.
        QStringList inner = m_shell;
        if (inner.isEmpty())
        {
            TerminalConfig plain(*this);
            plain.m_kind = MazeKind::Normal;
            inner = plain.getShell();
        }
        if (inner.isEmpty())
            inner << QStringLiteral("/bin/bash");

        QStringList run{QStringLiteral("entropy-shield-run"), QStringLiteral("--name"),
                        QStringLiteral("Tor terminal")};
        // A plain zsh or bash shows the connection log after its own start-up
        // output; anything else gets it before, as entropy-shield-run prints it.
        const QString shellName = QFileInfo(inner.first()).fileName();
        const bool plainShell = m_shell.isEmpty() && inner.size() == 1
            && (shellName == QLatin1String("zsh") || shellName == QLatin1String("bash"));
        if (extraEnv && plainShell && launcherHasLog())
        {
            const QString dir = torRcDir();
            const QString note = dir + QStringLiteral("/tor-%1.log")
                                 .arg(QUuid::createUuid().toString(QUuid::Id128));
            run << QStringLiteral("--log") << note;
            *extraEnv << QStringLiteral("MAZE_TERMINAL_NOTE=") + note;
            if (shellName == QLatin1String("zsh"))
            {
                *extraEnv << QStringLiteral("MAZE_USER_ZDOTDIR=") + QString::fromLocal8Bit(qgetenv("ZDOTDIR"))
                          << QStringLiteral("ZDOTDIR=") + dir + QStringLiteral("/zsh");
            }
            else
            {
                inner << QStringLiteral("--rcfile") << dir + QStringLiteral("/bashrc");
            }
        }
        return run + QStringList{QStringLiteral("--")} + inner;
    }
    if (!m_shell.isEmpty())
        return m_shell;
    if (!Properties::Instance()->shell.isEmpty())
        return Properties::Instance()->shell;
    QByteArray envShell = qgetenv("SHELL");
    if (envShell.constData() != nullptr)
    {
        QString shellString = QString::fromLocal8Bit(envShell).trimmed();
        if (!shellString.isEmpty())
            return QStringList{shellString};
    }
    return QStringList();
}

bool TerminalConfig::hasCommand() const
{
    return !m_shell.isEmpty();
}

void TerminalConfig::setWorkingDirectory(const QString &val)
{
    m_workingDirectory = val;
}

void TerminalConfig::setShell(const QStringList &val)
{
    m_shell = val;
}

void TerminalConfig::provideCurrentDirectory(const QString &val)
{
    m_currentDirectory = val;
}



#if HAVE_QDBUS

#define DBUS_ARG_WORKDIR "workingDirectory"
#define DBUS_ARG_SHELL "shell"
#define DBUS_ARG_KIND "kind"

TerminalConfig TerminalConfig::fromDbus(const QHash<QString,QVariant> &termArgsConst, TermWidget *toSplit)
{
    QHash<QString,QVariant> termArgs(termArgsConst);
    if (toSplit != nullptr && !termArgs.contains(QLatin1String(DBUS_ARG_WORKDIR)))
    {
        termArgs[QLatin1String(DBUS_ARG_WORKDIR)] = QVariant(toSplit->impl()->workingDirectory());
    }
    return TerminalConfig::fromDbus(termArgs);
}

static QString variantToString(const QVariant& variant, QString &defaultVal)
{
    if (variant.typeId() == QMetaType::QString)
        return qvariant_cast<QString>(variant);
    return defaultVal;
}

static QStringList variantToStringList(const QVariant& variant, QStringList &defaultVal)
{
    if (variant.typeId() == QMetaType::QStringList)
        return qvariant_cast<QStringList>(variant);
    return defaultVal;
}

TerminalConfig TerminalConfig::fromDbus(const QHash<QString,QVariant> &termArgs)
{
    QString wdir = QString();
    QStringList shell(Properties::Instance()->shell);
    if (termArgs.contains(QLatin1String(DBUS_ARG_WORKDIR)))
    {
        wdir = variantToString(termArgs[QLatin1String(DBUS_ARG_WORKDIR)], wdir);
    }
    if (termArgs.contains(QLatin1String(DBUS_ARG_SHELL))) {
        shell = variantToStringList(termArgs[QLatin1String(DBUS_ARG_SHELL)], shell);
    }
    TerminalConfig cfg(wdir, shell);
    // Maze: "kind" = "tor" or "incognito" opens that kind of tab.
    QString none;
    const QString kind = variantToString(termArgs.value(QLatin1String(DBUS_ARG_KIND)), none);
    if (kind == QLatin1String("tor"))
        cfg.setMazeKind(MazeKind::Tor);
    else if (kind == QLatin1String("incognito"))
        cfg.setMazeKind(MazeKind::Incognito);
    return cfg;
}

#endif

