#include "DownloadManager.h"
#include "AppSettings.h"
#include "Utils.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMessageBox>

DownloadManager *DownloadManager::instance()
{
    static DownloadManager *s_dm = new DownloadManager();
    return s_dm;
}

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_speedTimer, &QTimer::timeout, this, [this] {
        for (Item &it : m_items) {
            if (!it.req) continue;
            const qint64 nowRecv = it.req->receivedBytes();
            const qint64 delta = nowRecv - it.received;
            it.speed = qMax<qreal>(0, delta * 2); // 500ms window -> per second
            it.received = nowRecv;
        }
        const int rows = m_items.size();
        for (int i = 0; i < rows; ++i) emit rowChanged(i);
    });
    m_speedTimer.start(500);
}

void DownloadManager::attach(QWebEngineProfile *profile)
{
    if (m_profiles.contains(profile)) return;
    m_profiles.append(profile);
    connect(profile, &QWebEngineProfile::downloadRequested, this,
            &DownloadManager::onDownloadRequested);
}

void DownloadManager::detach(QWebEngineProfile *profile)
{
    m_profiles.removeOne(profile);
    disconnect(profile, &QWebEngineProfile::downloadRequested, this,
               &DownloadManager::onDownloadRequested);
}

void DownloadManager::onDownloadRequested(QWebEngineDownloadRequest *req)
{
    AppSettings *s = AppSettings::instance();
    QString dir = s->downloadDir();
    QString name = req->downloadFileName();
    if (name.isEmpty()) {
        name = QFileInfo(req->url().path()).fileName();
        if (name.isEmpty()) name = "download";
    }

    if (s->askWhereToSave()) {
        QString chosen = QFileDialog::getSaveFileName(
            qApp->activeWindow(), tr("Save file"), dir + "/" + name);
        if (chosen.isEmpty()) {
            req->cancel();
            return;
        }
        req->setDownloadDirectory(QFileInfo(chosen).absolutePath());
        req->setDownloadFileName(QFileInfo(chosen).fileName());
    } else {
        // de-duplicate: file (1).ext, file (2).ext ...
        QString base = name;
        QString ext;
        const int dot = name.lastIndexOf('.');
        if (dot > 0) {
            base = name.left(dot);
            ext = name.mid(dot);
        }
        QString candidate = base + ext;
        int i = 1;
        while (QFile::exists(dir + "/" + candidate))
            candidate = base + " (" + QString::number(i++) + ")" + ext;
        req->setDownloadDirectory(dir);
        req->setDownloadFileName(candidate);
    }

    Item it;
    it.req = req;
    it.url = req->url().toString();
    it.host = Utils::hostOfUrl(req->url());
    it.fileName = req->downloadFileName();
    it.state = int(req->state());
    m_items.prepend(it);

    // live updates
    connect(req, &QWebEngineDownloadRequest::stateChanged, this, [this, req]() {
        const int row = rowFor(req);
        if (row < 0) return;
        m_items[row].state = int(req->state());
        if (req->state() == QWebEngineDownloadRequest::DownloadCompleted) {
            m_items[row].filePath = req->downloadDirectory() + "/" + req->downloadFileName();
            emit downloadFinished(m_items[row].fileName, m_items[row].filePath, true);
        } else if (req->state() == QWebEngineDownloadRequest::DownloadCancelled) {
            emit downloadFinished(m_items[row].fileName, QString(), false);
        } else if (req->state() == QWebEngineDownloadRequest::DownloadInterrupted) {
            emit downloadFinished(m_items[row].fileName, QString(), false);
        }
        emit rowChanged(row);
        recomputeActive();
    });
    connect(req, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, req]() {
        const int row = rowFor(req);
        if (row < 0) return;
        m_items[row].received = req->receivedBytes();
        m_items[row].total = req->totalBytes();
        m_items[row].filePath = req->downloadDirectory() + "/" + req->downloadFileName();
        emit rowChanged(row);
    });
    connect(req, &QWebEngineDownloadRequest::totalBytesChanged, this, [this, req]() {
        const int row = rowFor(req);
        if (row < 0) return;
        m_items[row].total = req->totalBytes();
        emit rowChanged(row);
    });

    req->accept();
    recomputeActive();
    emit modelReset();
    emit downloadStarted(it.fileName);
    emit downloadsBadgeChanged(m_active);
}

void DownloadManager::recomputeActive()
{
    int a = 0;
    for (const Item &it : m_items) {
        if (it.state == int(QWebEngineDownloadRequest::DownloadInProgress)) a++;
    }
    if (a != m_active) {
        m_active = a;
        emit downloadsBadgeChanged(m_active);
    }
}

int DownloadManager::rowFor(QWebEngineDownloadRequest *req) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].req == req) return i;
    return -1;
}

void DownloadManager::pause(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    QWebEngineDownloadRequest *r = m_items[row].req;
    if (r && r->state() == QWebEngineDownloadRequest::DownloadInProgress && !r->isPaused())
        r->pause();
}

void DownloadManager::resume(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    QWebEngineDownloadRequest *r = m_items[row].req;
    if (r && r->isPaused())
        r->resume();
}

void DownloadManager::cancel(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    QWebEngineDownloadRequest *r = m_items[row].req;
    if (r && (r->state() == QWebEngineDownloadRequest::DownloadInProgress))
        r->cancel();
}

void DownloadManager::remove(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    m_items.removeAt(row);
    emit modelReset();
}

void DownloadManager::openFile(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    const Item &it = m_items[row];
    if (it.state == int(QWebEngineDownloadRequest::DownloadCompleted)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(it.filePath));
    } else if (!it.filePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(it.filePath));
    }
}

void DownloadManager::showInFolder(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    const QString dir = m_items[row].req ? m_items[row].req->downloadDirectory()
                                         : QFileInfo(m_items[row].filePath).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void DownloadManager::copyLink(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    qApp->clipboard()->setText(m_items[row].url);
}

// ---------------------------------------------------------------- model

DownloadsModel::DownloadsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(DownloadManager::instance(), &DownloadManager::modelReset, this, [this] {
        beginResetModel();
        endResetModel();
    });
    connect(DownloadManager::instance(), &DownloadManager::rowChanged, this, [this](int row) {
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    });
}

void DownloadsModel::refresh()
{
    beginResetModel();
    endResetModel();
}

int DownloadsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : DownloadManager::instance()->rowCount();
}

int DownloadsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant DownloadsModel::data(const QModelIndex &index, int role) const
{
    const DownloadManager::Item it = DownloadManager::instance()->itemAt(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Name:
            return it.fileName;
        case Size:
            return it.total > 0 ? Utils::formatBytes(it.total)
                                : (it.received > 0 ? Utils::formatBytes(it.received) : tr("Unknown"));
        case State:
            switch (it.state) {
            case QWebEngineDownloadRequest::DownloadRequested: return tr("Requested");
            case QWebEngineDownloadRequest::DownloadInProgress:
                return (it.req && it.req->isPaused()) ? tr("Paused") : tr("Downloading");
            case QWebEngineDownloadRequest::DownloadCompleted: return tr("Completed");
            case QWebEngineDownloadRequest::DownloadCancelled: return tr("Cancelled");
            case QWebEngineDownloadRequest::DownloadInterrupted: return tr("Failed");
            default: return tr("Unknown");
            }
        case Progress:
            return it.total > 0 ? int(it.received * 100 / it.total) : 0;
        }
    } else if (role == Qt::ToolTipRole) {
        return it.url;
    }
    return {};
}

QVariant DownloadsModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o == Qt::Horizontal && role == Qt::DisplayRole) {
        static const char *names[] = {QT_TRANSLATE_NOOP("Downloads", "File"),
                                      QT_TRANSLATE_NOOP("Downloads", "Size"),
                                      QT_TRANSLATE_NOOP("Downloads", "State"),
                                      QT_TRANSLATE_NOOP("Downloads", "%")};
        return tr(names[section]);
    }
    return {};
}
