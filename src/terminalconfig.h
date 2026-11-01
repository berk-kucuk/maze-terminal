
#ifndef TERMINALCONFIG_H
#define TERMINALCONFIG_H

#include <QHash>
#include <QString>
#include <QVariant>

class TermWidget;

// Maze Terminal: what a terminal is. A Tor terminal runs its shell through
// entropy-shield-run, so every command in it can only reach the internet
// through Tor; an incognito one keeps no shell history.
enum class MazeKind { Normal = 0, Tor = 1, Incognito = 2 };

class TerminalConfig
{
    public:
        TerminalConfig(const QString & wdir, const QStringList & shell);
        TerminalConfig(const TerminalConfig &cfg);
        TerminalConfig();

        QString getWorkingDirectory();
        // extraEnv, when given, receives environment entries the shell needs
        // (a Tor tab's start-up wrapper, see getShell()).
        QStringList getShell(QStringList *extraEnv = nullptr);
        bool hasCommand() const;

        void setWorkingDirectory(const QString &val);
        void setShell(const QStringList &val);
        void provideCurrentDirectory(const QString &val);

        MazeKind mazeKind() const { return m_kind; }
        void setMazeKind(MazeKind kind) { m_kind = kind; }

        #ifdef HAVE_QDBUS
        static TerminalConfig fromDbus(const QHash<QString,QVariant> &termArgs);
        static TerminalConfig fromDbus(const QHash<QString,QVariant> &termArgs, TermWidget *toSplit);
        #endif


    private:
    	// True when
    	QString m_currentDirectory;
    	QString m_workingDirectory;
        QStringList m_shell;
        MazeKind m_kind = MazeKind::Normal;
};

#endif
