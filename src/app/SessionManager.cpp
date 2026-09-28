#include "SessionManager.h"

#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

SessionManager *SessionManager::instance()
{
    static SessionManager *s = new SessionManager();
    return s;
}

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wed";
    QDir().mkpath(root);
    m_path = root + "/session.json";
    // Snapshot crash state once at process start, before any save happens.
    m_crashedLastRun = hasLastSession() && !readCleanFlag();
    if (!QFile::exists(m_path + ".clean"))
        m_cleanExit = readCleanFlag();
}

QString SessionManager::sessionFilePath() const
{
    return m_path;
}

bool SessionManager::readCleanFlag() const
{
    // Clean-exit marker lives beside the session file; absent = unclean.
    return QFile::exists(m_path + ".clean");
}

bool SessionManager::hasLastSession() const
{
    return QFile::exists(m_path) && QFileInfo(m_path).size() > 2;
}

SessionManager::SessionData SessionManager::loadLastSession() const
{
    SessionData d;
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return d;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return d;
    const QJsonObject root = doc.object();

    // format version guard
    if (root.value("version").toInt() != 1)
        return d;

    d.currentIndex = root.value("currentIndex").toInt(0);
    const QJsonArray tabs = root.value("tabs").toArray();
    for (const QJsonValue &v : tabs) {
        const QJsonObject t = v.toObject();
        TabWidget::TabSession s;
        s.url = QUrl(t.value("url").toString());
        s.pinned = t.value("pinned").toBool(false);
        s.muted = t.value("muted").toBool(false);
        for (const QJsonValue &b : t.value("back").toArray())
            s.back << b.toString();
        for (const QJsonValue &fw : t.value("forward").toArray())
            s.forward << fw.toString();
        if (!s.url.isEmpty() || !s.back.isEmpty())
            d.tabs.append(s);
    }
    if (d.currentIndex < 0 || d.currentIndex >= d.tabs.size())
        d.currentIndex = d.tabs.isEmpty() ? 0 : d.tabs.size() - 1;
    return d;
}

void SessionManager::save(const QList<TabWidget::TabSession> &tabs, int currentIndex)
{
    // unclean while running: remove marker so a kill now counts as crash
    QFile::remove(m_path + ".clean");

    QJsonObject root;
    root.insert("version", 1);
    root.insert("currentIndex", currentIndex);
    QJsonArray arr;
    for (const TabWidget::TabSession &s : tabs) {
        QJsonObject t;
        t.insert("url", s.url.toString());
        t.insert("pinned", s.pinned);
        t.insert("muted", s.muted);
        QJsonArray back, fwd;
        for (const QUrl &u : s.back) back.append(u.toString());
        for (const QUrl &u : s.forward) fwd.append(u.toString());
        t.insert("back", back);
        t.insert("forward", fwd);
        arr.append(t);
    }
    root.insert("tabs", arr);

    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.commit();
}

void SessionManager::markCleanExit()
{
    QFile f(m_path + ".clean");
    if (f.open(QIODevice::WriteOnly)) {
        f.write("1");
        f.close();
    }
}

void SessionManager::clearSession()
{
    QFile::remove(m_path);
    QFile::remove(m_path + ".clean");
}
