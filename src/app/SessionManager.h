#pragma once
#include <QObject>
#include <QList>
#include "TabWidget.h"

// Crash-safe session persistence. Session file is written atomically (QSaveFile)
// on a debounce timer and on close. A clean-exit flag distinguishes normal
// shutdown from crash/kill for the "restore session?" recovery flow.
class SessionManager : public QObject
{
    Q_OBJECT
public:
    struct SessionData {
        QList<TabWidget::TabSession> tabs;
        int currentIndex = 0;
    };

    static SessionManager *instance();

    bool hasLastSession() const;
    SessionData loadLastSession() const;
    void save(const QList<TabWidget::TabSession> &tabs, int currentIndex);
    void markCleanExit();
    void clearSession();

    // true when the previous run terminated without markCleanExit() and a
    // session file exists (crash, kill, power loss)
    bool crashedLastRun() const { return m_crashedLastRun; }

    QString sessionFilePath() const;

private:
    explicit SessionManager(QObject *parent = nullptr);
    bool readCleanFlag() const;

    QString m_path;          // <AppData>/wed/session.json
    bool m_crashedLastRun = false;
    bool m_cleanExit = true; // current-run state, flushed by markCleanExit
};
