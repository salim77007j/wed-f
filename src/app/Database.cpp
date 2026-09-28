#include "Database.h"
#include "AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QStandardPaths>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QUrl>

Database *Database::instance(const QString &path)
{
    static Database *s_db = new Database(path);
    return s_db;
}

Database::Database(const QString &path, QObject *parent)
    : QObject(parent)
{
    if (path.isEmpty()) {
        m_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + "/wed/browser.db";
    } else {
        m_path = path;
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    m_db = QSqlDatabase::addDatabase("QSQLITE", "wed");
    m_db.setDatabaseName(m_path);
    if (!m_db.open())
        qWarning("Database open failed: %s", qPrintable(m_db.lastError().text()));

    exec("PRAGMA journal_mode=WAL");
    exec("CREATE TABLE IF NOT EXISTS history ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "url TEXT NOT NULL,"
          "title TEXT,"
          "visit_time INTEGER NOT NULL,"
          "host TEXT)");
    exec("CREATE INDEX IF NOT EXISTS idx_history_host ON history(host)");
    exec("CREATE INDEX IF NOT EXISTS idx_history_time ON history(visit_time DESC)");
    exec("CREATE TABLE IF NOT EXISTS bookmarks ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "parent INTEGER NOT NULL DEFAULT 0,"
          "title TEXT,"
          "url TEXT,"
          "pos INTEGER DEFAULT 0,"
          "folder INTEGER DEFAULT 0,"
          "created INTEGER)");
    exec("CREATE TABLE IF NOT EXISTS permissions ("
          "origin TEXT NOT NULL,"
          "feature TEXT NOT NULL,"
          "policy INTEGER NOT NULL,"
          "PRIMARY KEY(origin, feature))");
    exec("CREATE TABLE IF NOT EXISTS shield_exceptions ("
          "host TEXT PRIMARY KEY)");
    exec("CREATE TABLE IF NOT EXISTS site_stats ("
          "host TEXT PRIMARY KEY,"
          "ads INTEGER DEFAULT 0,"
          "trackers INTEGER DEFAULT 0,"
          "cookies INTEGER DEFAULT 0,"
          "updated INTEGER)");
    exec("CREATE TABLE IF NOT EXISTS zoom (host TEXT PRIMARY KEY, zoom INTEGER)");
    exec("CREATE TABLE IF NOT EXISTS closed_tabs ("
          "url TEXT, title TEXT, closed INTEGER)");
}

bool Database::exec(const QString &sql) const
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        qWarning("SQL failed: %s\n%s", qPrintable(sql), qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

QSqlQuery Database::query() const { return QSqlQuery(m_db); }

// ---------------------------------------------------------------- history

void Database::addHistory(const QString &url, const QString &title)
{
    if (url.startsWith("wed://")) return;
    QUrl u(url);
    QString host = Utils::hostOfUrl(u);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO history (url, title, visit_time, host) VALUES (?,?,?,?)");
    q.addBindValue(url);
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(host);
    q.exec();
}

QList<Database::HistoryEntry> Database::recentHistory(int limit) const
{
    QList<HistoryEntry> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, url, title, visit_time, COUNT(*) c FROM history "
              "GROUP BY url ORDER BY visit_time DESC LIMIT ?");
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryEntry e;
        e.id = q.value(0).toLongLong();
        e.url = q.value(1).toString();
        e.title = q.value(2).toString();
        e.visited = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        e.visits = q.value(4).toInt();
        e.host = Utils::hostOfUrl(QUrl(e.url));
        out.append(e);
    }
    return out;
}

QList<Database::HistoryEntry> Database::searchHistory(const QString &needle, int limit) const
{
    QList<HistoryEntry> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, url, title, MAX(visit_time) FROM history "
              "WHERE url LIKE ? OR title LIKE ? "
              "GROUP BY url ORDER BY visit_time DESC LIMIT ?");
    QString like = "%" + needle + "%";
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryEntry e;
        e.id = q.value(0).toLongLong();
        e.url = q.value(1).toString();
        e.title = q.value(2).toString();
        e.visited = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        e.visits = 1;
        e.host = Utils::hostOfUrl(QUrl(e.url));
        out.append(e);
    }
    return out;
}

QList<Database::HistoryEntry> Database::topSites(int limit) const
{
    QList<HistoryEntry> out;
    QSqlQuery q(m_db);
    // top sites by distinct days visited, most recent visit wins
    q.prepare("SELECT url, MAX(title), COUNT(*) visits, MAX(visit_time) FROM history "
              "WHERE host != '' GROUP BY host ORDER BY visits DESC, MAX(visit_time) DESC LIMIT ?");
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryEntry e;
        e.id = 0;
        e.url = q.value(0).toString();
        e.title = q.value(1).toString();
        e.visits = q.value(2).toInt();
        e.visited = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        e.host = Utils::hostOfUrl(QUrl(e.url));
        out.append(e);
    }
    return out;
}

void Database::deleteHistoryForUrl(const QString &url)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM history WHERE url = ?");
    q.addBindValue(url);
    q.exec();
}

void Database::clearHistory(qint64 beforeEpochMs)
{
    if (beforeEpochMs == 0) {
        exec("DELETE FROM history");
    } else {
        QSqlQuery q(m_db);
        q.prepare("DELETE FROM history WHERE visit_time < ?");
        q.addBindValue(beforeEpochMs);
        q.exec();
    }
    exec("DELETE FROM closed_tabs");
}

int Database::historyCount() const
{
    QSqlQuery q("SELECT COUNT(DISTINCT url) FROM history", m_db);
    return q.next() ? q.value(0).toInt() : 0;
}

QList<Database::HistoryEntry> Database::mostVisited(int days, int limit) const
{
    QList<HistoryEntry> out;
    QSqlQuery q(m_db);
    qint64 since = QDateTime::currentDateTime().addDays(-days).toMSecsSinceEpoch();
    q.prepare("SELECT url, MAX(title), COUNT(*) v, MAX(visit_time) FROM history "
              "WHERE visit_time > ? GROUP BY url ORDER BY v DESC LIMIT ?");
    q.addBindValue(since);
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryEntry e;
        e.id = 0;
        e.url = q.value(0).toString();
        e.title = q.value(1).toString();
        e.visits = q.value(2).toInt();
        e.visited = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        e.host = Utils::hostOfUrl(QUrl(e.url));
        out.append(e);
    }
    return out;
}

// ---------------------------------------------------------------- bookmarks

qint64 Database::addBookmark(qint64 parent, const QString &title, const QString &url, bool folder)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO bookmarks (parent, title, url, pos, folder, created) "
              "VALUES (?,?,?,(SELECT COALESCE(MAX(pos),0)+1 FROM bookmarks WHERE parent=?),?,?)");
    q.addBindValue(parent);
    q.addBindValue(title);
    q.addBindValue(url);
    q.addBindValue(parent);
    q.addBindValue(folder ? 1 : 0);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
    return q.lastInsertId().toLongLong();
}

bool Database::updateBookmark(qint64 id, const QString &title, const QString &url)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET title=?, url=? WHERE id=?");
    q.addBindValue(title);
    q.addBindValue(url);
    q.addBindValue(id);
    return q.exec();
}

bool Database::updateBookmarkByUrl(const QString &url, const QString &title)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET title=? WHERE url=? AND folder=0");
    q.addBindValue(title);
    q.addBindValue(url);
    return q.exec();
}

void Database::moveBookmark(qint64 id, qint64 newParent)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET parent=? WHERE id=?");
    q.addBindValue(newParent);
    q.addBindValue(id);
    q.exec();
}

void Database::removeBookmark(qint64 id)
{
    // remove children if folder
    QList<BookmarkNode> kids = bookmarks(id);
    for (const BookmarkNode &k : kids) removeBookmark(k.id);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM bookmarks WHERE id=?");
    q.addBindValue(id);
    q.exec();
}

QList<Database::BookmarkNode> Database::bookmarks(qint64 parent) const
{
    QList<BookmarkNode> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, parent, title, url, pos, folder FROM bookmarks "
              "WHERE parent=? ORDER BY folder DESC, pos");
    q.addBindValue(parent);
    q.exec();
    while (q.next()) {
        BookmarkNode n;
        n.id = q.value(0).toLongLong();
        n.parent = q.value(1).toLongLong();
        n.title = q.value(2).toString();
        n.url = q.value(3).toString();
        n.pos = q.value(4).toInt();
        n.folder = q.value(5).toInt() == 1;
        out.append(n);
    }
    return out;
}

QList<Database::BookmarkNode> Database::allBookmarks() const
{
    QList<BookmarkNode> out;
    QSqlQuery q(m_db);
    q.exec("SELECT id, parent, title, url, pos, folder FROM bookmarks ORDER BY folder DESC, pos");
    while (q.next()) {
        BookmarkNode n;
        n.id = q.value(0).toLongLong();
        n.parent = q.value(1).toLongLong();
        n.title = q.value(2).toString();
        n.url = q.value(3).toString();
        n.pos = q.value(4).toInt();
        n.folder = q.value(5).toInt() == 1;
        out.append(n);
    }
    return out;
}

bool Database::isBookmarked(const QString &url) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM bookmarks WHERE url=? AND folder=0 LIMIT 1");
    q.addBindValue(url);
    q.exec();
    return q.next();
}

qint64 Database::folderForUrl(const QString &url) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT parent FROM bookmarks WHERE url=? AND folder=0 LIMIT 1");
    q.addBindValue(url);
    q.exec();
    return q.next() ? q.value(0).toLongLong() : 0;
}

QString Database::bookmarkTitle(const QString &url) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT title FROM bookmarks WHERE url=? AND folder=0 LIMIT 1");
    q.addBindValue(url);
    q.exec();
    return q.next() ? q.value(0).toString() : QString();
}

int Database::bookmarkCount() const
{
    QSqlQuery q("SELECT COUNT(*) FROM bookmarks WHERE folder=0", m_db);
    return q.next() ? q.value(0).toInt() : 0;
}

QString Database::exportBookmarksJson() const
{
    QJsonArray arr;
    for (const BookmarkNode &n : allBookmarks()) {
        QJsonObject o;
        o["id"] = double(n.id);
        o["parent"] = double(n.parent);
        o["title"] = n.title;
        o["url"] = n.url;
        o["folder"] = n.folder;
        arr.append(o);
    }
    QJsonObject root;
    root["bookmarks"] = arr;
    return QString::fromUtf8(QJsonDocument(root).toJson());
}

bool Database::importBookmarksJson(const QString &json)
{
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;
    const QJsonArray arr = doc.object().value("bookmarks").toArray();
    // map old ids to new ids preserving hierarchy
    QMap<qint64, qint64> idmap;
    idmap.insert(0, 0);
    for (int pass = 0; pass < 3; ++pass) {
        for (const QJsonValue &v : arr) {
            QJsonObject o = v.toObject();
            qint64 oldId = qint64(o["id"].toDouble());
            qint64 oldParent = qint64(o["parent"].toDouble());
            if (!idmap.contains(oldParent)) continue;
            if (idmap.contains(oldId)) continue;
            qint64 newId = addBookmark(idmap.value(oldParent), o["title"].toString(),
                                       o["url"].toString(), o["folder"].toBool());
            idmap.insert(oldId, newId);
        }
    }
    return true;
}

// ---------------------------------------------------------------- permissions

void Database::setPermission(const QString &origin, const QString &feature, int policy)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO permissions (origin, feature, policy) VALUES (?,?,?)");
    q.addBindValue(origin);
    q.addBindValue(feature);
    q.addBindValue(policy);
    q.exec();
}

int Database::permission(const QString &origin, const QString &feature, int def) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT policy FROM permissions WHERE origin=? AND feature=?");
    q.addBindValue(origin);
    q.addBindValue(feature);
    q.exec();
    return q.next() ? q.value(0).toInt() : def;
}

QList<Database::PermissionRow> Database::allPermissions() const
{
    QList<PermissionRow> out;
    QSqlQuery q(m_db);
    q.exec("SELECT origin, feature, policy FROM permissions ORDER BY origin");
    while (q.next())
        out.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toInt()});
    return out;
}

void Database::removePermission(const QString &origin, const QString &feature)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM permissions WHERE origin=? AND feature=?");
    q.addBindValue(origin);
    q.addBindValue(feature);
    q.exec();
}

// ---------------------------------------------------------------- shields

void Database::setShieldException(const QString &host, bool off)
{
    QSqlQuery q(m_db);
    if (off) {
        q.prepare("INSERT OR REPLACE INTO shield_exceptions (host) VALUES (?)");
        q.addBindValue(host);
    } else {
        q.prepare("DELETE FROM shield_exceptions WHERE host=?");
        q.addBindValue(host);
    }
    q.exec();
}

bool Database::isShieldOff(const QString &host) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM shield_exceptions WHERE host=?");
    q.addBindValue(host);
    q.exec();
    return q.next();
}

QStringList Database::shieldExceptions() const
{
    QStringList out;
    QSqlQuery q(m_db);
    q.exec("SELECT host FROM shield_exceptions");
    while (q.next()) out << q.value(0).toString();
    return out;
}

// ---------------------------------------------------------------- stats

void Database::incrementStats(const QString &host, int ads, int trackers, int cookies)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO site_stats (host, ads, trackers, cookies, updated) VALUES (?,?,?,?,?) "
              "ON CONFLICT(host) DO UPDATE SET ads=ads+?, trackers=trackers+?, cookies=cookies+?, updated=?");
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    q.addBindValue(host);
    q.addBindValue(ads);
    q.addBindValue(trackers);
    q.addBindValue(cookies);
    q.addBindValue(now);
    q.addBindValue(ads);
    q.addBindValue(trackers);
    q.addBindValue(cookies);
    q.addBindValue(now);
    q.exec();
}

void Database::clearStats()
{
    exec("DELETE FROM site_stats");
}

QList<Database::SiteStats> Database::siteStats(int limit) const
{
    QList<SiteStats> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT host, ads, trackers, cookies FROM site_stats "
              "ORDER BY (ads+trackers) DESC LIMIT ?");
    q.addBindValue(limit);
    q.exec();
    while (q.next())
        out.append({q.value(0).toString(), q.value(1).toInt(), q.value(2).toInt(), q.value(3).toInt()});
    return out;
}

Database::SiteStats Database::totals() const
{
    SiteStats t{"TOTAL", 0, 0, 0};
    QSqlQuery q("SELECT COALESCE(SUM(ads),0), COALESCE(SUM(trackers),0), COALESCE(SUM(cookies),0) FROM site_stats", m_db);
    if (q.next()) {
        t.ads = q.value(0).toInt();
        t.trackers = q.value(1).toInt();
        t.cookies = q.value(2).toInt();
    }
    return t;
}

Database::SiteStats Database::totalsFor(const QString &host) const
{
    SiteStats t{host, 0, 0, 0};
    QSqlQuery q(m_db);
    q.prepare("SELECT ads, trackers, cookies FROM site_stats WHERE host=?");
    q.addBindValue(host);
    q.exec();
    if (q.next()) {
        t.ads = q.value(0).toInt();
        t.trackers = q.value(1).toInt();
        t.cookies = q.value(2).toInt();
    }
    return t;
}

// ---------------------------------------------------------------- zoom / closed

void Database::setZoom(const QString &host, int zoom)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO zoom (host, zoom) VALUES (?,?)");
    q.addBindValue(host);
    q.addBindValue(zoom);
    q.exec();
}

int Database::zoom(const QString &host, int def) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT zoom FROM zoom WHERE host=?");
    q.addBindValue(host);
    q.exec();
    return q.next() ? q.value(0).toInt() : def;
}

void Database::pushClosedTab(const QString &url, const QString &title)
{
    if (url.isEmpty() || url.startsWith("wed://")) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO closed_tabs (url, title, closed) VALUES (?,?,?)");
    q.addBindValue(url);
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
}

QStringList Database::closedTabs(int limit) const
{
    QStringList out;
    QSqlQuery q(m_db);
    q.prepare("SELECT url, title FROM closed_tabs ORDER BY closed DESC LIMIT ?");
    q.addBindValue(limit);
    q.exec();
    while (q.next()) out << q.value(0).toString() + "|" + q.value(1).toString();
    return out;
}

void Database::clearClosedTabs()
{
    exec("DELETE FROM closed_tabs");
}

// ---------------------------------------------------------------- models

HistoryTableModel::HistoryTableModel(QObject *parent)
    : QAbstractTableModel(parent) {}

void HistoryTableModel::reload(const QString &search)
{
    beginResetModel();
    m_rows = search.isEmpty() ? Database::instance()->recentHistory(1000)
                              : Database::instance()->searchHistory(search);
    endResetModel();
}

int HistoryTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int HistoryTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant HistoryTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const Database::HistoryEntry &e = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return e.title.isEmpty() ? e.host : e.title;
        case 1: return e.url;
        case 2: return e.visited.toString("yyyy-MM-dd hh:mm");
        }
    } else if (role == UrlRole) {
        return e.url;
    } else if (role == TitleRole) {
        return e.title;
    } else if (role == Qt::ToolTipRole) {
        return e.url;
    }
    return {};
}

QVariant HistoryTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o == Qt::Horizontal && role == Qt::DisplayRole) {
        static const char *names[] = {"Page", "Address", "When"};
        return QString::fromUtf8(names[section]);
    }
    return {};
}

BookmarksModel::BookmarksModel(QObject *parent)
    : QAbstractTableModel(parent) {}

void BookmarksModel::reload()
{
    beginResetModel();
    m_rows = Database::instance()->allBookmarks();
    endResetModel();
}

int BookmarksModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int BookmarksModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

QVariant BookmarksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const Database::BookmarkNode &n = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        if (index.column() == 0) return n.folder ? n.title : (n.title.isEmpty() ? n.url : n.title);
        if (index.column() == 1) return n.folder ? QString() : n.url;
    } else if (role == Qt::ToolTipRole) {
        return n.url;
    }
    return {};
}

QVariant BookmarksModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o == Qt::Horizontal && role == Qt::DisplayRole)
        return section == 0 ? QStringLiteral("Name") : QStringLiteral("Address");
    return {};
}
