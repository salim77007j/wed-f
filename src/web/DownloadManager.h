#pragma once
#include <QObject>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QAbstractTableModel>
#include <QTimer>
#include <QQueue>
#include <QtWebEngineCore/QWebEngineDownloadRequest>

class QWebEngineDownloadRequest;
class QProgressDialog;

// Real download manager: intercepts profile downloadRequested, asks where to
// save (option), tracks progress/pause/resume/cancel, raises toasts.
class DownloadManager : public QObject
{
    Q_OBJECT
public:
    struct Item {
        QWebEngineDownloadRequest *req = nullptr;
        QString fileName;
        QString filePath;
        QString url;
        QString host;
        qint64 received = 0;
        qint64 total = 0;
        int state = 0;   // QWebEngineDownloadRequest::DownloadState
        qreal speed = 0; // bytes/s
    };

    static DownloadManager *instance();
    void attach(QWebEngineProfile *profile);
    void detach(QWebEngineProfile *profile);

    int activeCount() const { return m_active; }
    QList<Item> items() const { return m_items; }
    Item itemAt(int row) const { return m_items.value(row); }
    int rowCount() const { return m_items.size(); }

    void pause(int row);
    void resume(int row);
    void cancel(int row);
    void remove(int row);
    void openFile(int row);
    void showInFolder(int row);
    void copyLink(int row);

signals:
    void modelReset();
    void rowChanged(int row);
    void downloadsBadgeChanged(int active);
    void downloadStarted(const QString &fileName);
    void downloadFinished(const QString &fileName, const QString &path, bool ok);

private slots:
    void onDownloadRequested(QWebEngineDownloadRequest *req);

private:
    explicit DownloadManager(QObject *parent = nullptr);
    int rowFor(QWebEngineDownloadRequest *req) const;
    void recomputeActive();
    QList<Item> m_items;
    QList<QWebEngineProfile *> m_profiles;
    int m_active = 0;
    QTimer m_speedTimer;
    QHash<QWebEngineDownloadRequest *, QPair<qint64, qint64>> m_speedWindow;
};

// Table model backing the downloads panel
class DownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Name = 0, Size = 1, State = 2, Progress = 3 };
    explicit DownloadsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    void refresh();
signals:
    void progressTick();
};
