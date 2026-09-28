#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QAbstractTableModel>
#include <QList>

// SQLite persistence: history, bookmarks, permissions, per-site privacy stats,
// per-site zoom, shield exceptions. Single connection shared app-wide.
class Database : public QObject
{
    Q_OBJECT
public:
    static Database *instance(const QString &path = QString());

    struct HistoryEntry { qint64 id; QString url; QString title; QDateTime visited; int visits; QString host; };
    struct BookmarkNode { qint64 id; qint64 parent; QString title; QString url; int pos; bool folder; };
    struct SiteStats { QString host; int ads; int trackers; int cookiesBlocked; };
    struct PermissionRow { QString origin; QString feature; int policy; };

    // history
    void addHistory(const QString &url, const QString &title);
    QList<HistoryEntry> recentHistory(int limit = 500) const;
    QList<HistoryEntry> searchHistory(const QString &needle, int limit = 200) const;
    QList<HistoryEntry> topSites(int limit = 10) const;
    void clearHistory(qint64 beforeEpochMs = 0); // 0 = everything
    void deleteHistoryForUrl(const QString &url);
    int historyCount() const;
    QString lastUrlForHostPrefix(const QString &prefix) const;
    QList<HistoryEntry> mostVisited(int days, int limit) const;

    // bookmarks
    qint64 addBookmark(qint64 parent, const QString &title, const QString &url, bool folder = false);
    bool updateBookmark(qint64 id, const QString &title, const QString &url);
    bool updateBookmarkByUrl(const QString &url, const QString &title);
    void moveBookmark(qint64 id, qint64 newParent);
    void removeBookmark(qint64 id);
    QList<BookmarkNode> bookmarks(qint64 parent) const;
    QList<BookmarkNode> allBookmarks() const;
    bool isBookmarked(const QString &url) const;
    qint64 folderForUrl(const QString &url) const;
    QString bookmarkTitle(const QString &url) const;
    int bookmarkCount() const;
    bool importBookmarksJson(const QString &json);
    QString exportBookmarksJson() const;

    // permissions (origin+feature -> policy 0 ask /1 allow /2 block)
    void setPermission(const QString &origin, const QString &feature, int policy);
    int permission(const QString &origin, const QString &feature, int def = 0) const;
    QList<PermissionRow> allPermissions() const;
    void removePermission(const QString &origin, const QString &feature);

    // shield exceptions (protections off for a site)
    void setShieldException(const QString &host, bool off);
    bool isShieldOff(const QString &host) const;
    QStringList shieldExceptions() const;

    // per-site privacy stats
    void clearStats();
    void incrementStats(const QString &host, int ads, int trackers, int cookies);
    QList<SiteStats> siteStats(int limit = 50) const;
    SiteStats totals() const;
    SiteStats totalsFor(const QString &host) const;

    // per-site zoom
    void setZoom(const QString &host, int zoom);
    int zoom(const QString &host, int def = 100) const;

    // recently closed tabs (session history)
    void pushClosedTab(const QString &url, const QString &title);
    QStringList closedTabs(int limit = 10) const; // url|title
    void clearClosedTabs();

    QString databasePath() const { return m_path; }

private:
    explicit Database(const QString &path, QObject *parent = nullptr);
    bool exec(const QString &sql) const;
    QSqlQuery query() const;
    QString m_path;
    QSqlDatabase m_db;
};

// ---------------------------------------------------------------- models
class HistoryTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit HistoryTableModel(QObject *parent = nullptr);
    enum { UrlRole = Qt::UserRole + 1, TitleRole, VisitedRole };
    void reload(const QString &search = QString());
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    const Database::HistoryEntry &entryAt(int row) const { return m_rows.at(row); }
private:
    QList<Database::HistoryEntry> m_rows;
};

class BookmarksModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit BookmarksModel(QObject *parent = nullptr);
    void reload();
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    const Database::BookmarkNode &nodeAt(int row) const { return m_rows.at(row); }
private:
    QList<Database::BookmarkNode> m_rows;
};
