#include "Panels.h"
#include "AppSettings.h"
#include "Database.h"
#include "DownloadManager.h"
#include "MainWindow.h"
#include "ThemeManager.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

// ---------------------------------------------------------------- downloads

DownloadsPanel::DownloadsPanel(MainWindow *window, QWidget *parent)
    : QWidget(parent), m_window(window)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_model = new DownloadsModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 90);
    m_table->setColumnWidth(2, 110);
    m_table->setColumnWidth(3, 60);
    m_table->setMinimumHeight(180);
    connect(m_table, &QTableView::customContextMenuRequested, this, &DownloadsPanel::onCustomMenu);
    connect(m_table, &QTableView::doubleClicked, this, &DownloadsPanel::onDoubleClicked);
    lay->addWidget(m_table);

    auto *row = new QHBoxLayout();
    auto *clear = new QPushButton(tr("Clear list"), this);
    connect(clear, &QPushButton::clicked, this, [] {
        while (DownloadManager::instance()->rowCount() > 0)
            DownloadManager::instance()->remove(0);
    });
    row->addWidget(clear);
    row->addStretch(1);
    lay->addLayout(row);
}

void DownloadsPanel::reload()
{
    m_model->refresh();
}

void DownloadsPanel::onDoubleClicked(const QModelIndex &idx)
{
    DownloadManager::instance()->openFile(idx.row());
}

void DownloadsPanel::onCustomMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;
    const int row = idx.row();
    const DownloadManager::Item it = DownloadManager::instance()->itemAt(row);
    QMenu m(this);
    if (it.state == int(QWebEngineDownloadRequest::DownloadInProgress)) {
        if (it.req && it.req->isPaused()) {
            QAction *r = m.addAction(tr("Resume"));
            connect(r, &QAction::triggered, this, [row] { DownloadManager::instance()->resume(row); });
        } else {
            QAction *p = m.addAction(tr("Pause"));
            connect(p, &QAction::triggered, this, [row] { DownloadManager::instance()->pause(row); });
        }
        QAction *c = m.addAction(tr("Cancel"));
        connect(c, &QAction::triggered, this, [row] { DownloadManager::instance()->cancel(row); });
        m.addSeparator();
    }
    QAction *open = m.addAction(tr("Open"));
    connect(open, &QAction::triggered, this, [row] { DownloadManager::instance()->openFile(row); });
    QAction *folder = m.addAction(tr("Show in folder"));
    connect(folder, &QAction::triggered, this, [row] { DownloadManager::instance()->showInFolder(row); });
    QAction *copy = m.addAction(tr("Copy download link"));
    connect(copy, &QAction::triggered, this, [row] { DownloadManager::instance()->copyLink(row); });
    m.addSeparator();
    QAction *rm = m.addAction(tr("Remove from list"));
    connect(rm, &QAction::triggered, this, [row] { DownloadManager::instance()->remove(row); });
    m.exec(m_table->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------- history

HistoryPanel::HistoryPanel(MainWindow *window, QWidget *parent)
    : QWidget(parent), m_window(window)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search history"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &HistoryPanel::onSearchChanged);
    lay->addWidget(m_search);

    m_model = new HistoryTableModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(2, 140);
    m_table->setMinimumHeight(260);
    connect(m_table, &QTableView::doubleClicked, this, &HistoryPanel::onDoubleClicked);
    connect(m_table, &QTableView::customContextMenuRequested, this, &HistoryPanel::onCustomMenu);
    lay->addWidget(m_table, 1);

    auto *row = new QHBoxLayout();
    auto *clear = new QPushButton(tr("Clear browsing data…"), this);
    connect(clear, &QPushButton::clicked, m_window, &MainWindow::showClearBrowsingData);
    row->addWidget(clear);
    row->addStretch(1);
    lay->addLayout(row);

    m_model->reload();
}

void HistoryPanel::reload() { m_model->reload(m_search ? m_search->text() : QString()); }

void HistoryPanel::onSearchChanged(const QString &text) { m_model->reload(text); }

void HistoryPanel::onDoubleClicked(const QModelIndex &idx)
{
    const QString url = idx.data(HistoryTableModel::UrlRole).toString();
    if (!url.isEmpty())
        emit openUrlRequested(QUrl(url), false);
}

void HistoryPanel::onCustomMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;
    const QString url = idx.data(HistoryTableModel::UrlRole).toString();
    QMenu m(this);
    QAction *open = m.addAction(tr("Open in new tab"));
    connect(open, &QAction::triggered, this, [this, url] { emit openUrlRequested(QUrl(url), false); });
    QAction *openBg = m.addAction(tr("Open in background tab"));
    connect(openBg, &QAction::triggered, this, [this, url] { emit openUrlRequested(QUrl(url), true); });
    m.addSeparator();
    QAction *copy = m.addAction(tr("Copy address"));
    connect(copy, &QAction::triggered, this, [url] { qApp->clipboard()->setText(url); });
    m.addSeparator();
    QAction *del = m.addAction(Icons::themed("trash"), tr("Delete from history"));
    connect(del, &QAction::triggered, this, [this, idx] {
        const QString u = idx.data(HistoryTableModel::UrlRole).toString();
        Database::instance()->deleteHistoryForUrl(u);
        reload();
    });
    m.exec(m_table->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------- bookmarks

BookmarksPanel::BookmarksPanel(MainWindow *window, QWidget *parent)
    : QWidget(parent), m_window(window)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({tr("Bookmarks")});
    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setMinimumHeight(260);
    connect(m_tree, &QTreeView::activated, this, &BookmarksPanel::onItemActivated);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, &BookmarksPanel::onCustomMenu);
    lay->addWidget(m_tree, 1);

    auto *row = new QHBoxLayout();
    auto *addFolder = new QPushButton(tr("New folder"), this);
    connect(addFolder, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New folder"), tr("Name:"), QLineEdit::Normal, tr("New folder"), &ok);
        if (ok && !name.isEmpty()) {
            QModelIndex sel = m_tree->currentIndex();
            qint64 parent = 0;
            if (sel.isValid()) {
                QVariant v = sel.data(Qt::UserRole + 2);
                if (v.isValid()) parent = v.toLongLong();
            }
            Database::instance()->addBookmark(parent, name, QString(), true);
            reload();
        }
    });
    row->addWidget(addFolder);

    auto *importBtn = new QPushButton(tr("Import…"), this);
    connect(importBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("Import bookmarks"), QString(), "JSON (*.json)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly)) return;
        const QString json = QString::fromUtf8(file.readAll());
        if (Database::instance()->importBookmarksJson(json))
            reload();
        else
            QMessageBox::warning(this, tr("Import failed"), tr("Could not parse the bookmarks file."));
    });
    row->addWidget(importBtn);

    auto *exportBtn = new QPushButton(tr("Export…"), this);
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("Export bookmarks"), "wed-bookmarks.json", "JSON (*.json)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(Database::instance()->exportBookmarksJson().toUtf8());
            file.close();
        }
    });
    row->addWidget(exportBtn);
    row->addStretch(1);
    lay->addLayout(row);

    reload();
}

void BookmarksPanel::buildTree()
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels({tr("Bookmarks")});

    // collect all nodes, build tree by parent
    QHash<qint64, QStandardItem *> map;
    map.insert(0, m_model->invisibleRootItem());
    for (int pass = 0; pass < 4; ++pass) {
        for (const Database::BookmarkNode &n : Database::instance()->allBookmarks()) {
            if (map.contains(n.id) || !map.contains(n.parent)) continue;
            auto *item = new QStandardItem(n.title.isEmpty() ? n.url : n.title);
            item->setIcon(n.folder ? Icons::themed("folder") : Icons::themed("star"));
            item->setEditable(true);
            item->setData(n.id, Qt::UserRole + 1);
            item->setData(n.parent, Qt::UserRole + 2);
            item->setData(n.url, Qt::UserRole + 3);
            item->setToolTip(n.url);
            map.value(n.parent)->appendRow(item);
            map.insert(n.id, item);
        }
    }
    m_tree->expandAll();
}

void BookmarksPanel::reload() { buildTree(); }

void BookmarksPanel::onItemActivated(const QModelIndex &idx)
{
    const QString url = idx.data(Qt::UserRole + 3).toString();
    if (!url.isEmpty())
        emit openUrlRequested(QUrl(url));
}

void BookmarksPanel::onCustomMenu(const QPoint &pos)
{
    const QModelIndex idx = m_tree->indexAt(pos);
    QMenu m(this);
    if (!idx.isValid()) return;
    const qint64 id = idx.data(Qt::UserRole + 1).toLongLong();
    const QString url = idx.data(Qt::UserRole + 3).toString();

    if (!url.isEmpty()) {
        QAction *open = m.addAction(tr("Open in new tab"));
        connect(open, &QAction::triggered, this, [this, url] { emit openUrlRequested(QUrl(url)); });
        QAction *copy = m.addAction(tr("Copy address"));
        connect(copy, &QAction::triggered, this, [url] { qApp->clipboard()->setText(url); });
        m.addSeparator();
    }
    QAction *rename = m.addAction(Icons::themed("edit"), tr("Rename…"));
    connect(rename, &QAction::triggered, this, [this, idx, id] {
        const QString oldName = idx.data().toString();
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename"), tr("Name:"), QLineEdit::Normal, oldName, &ok);
        if (ok && !name.isEmpty()) {
            // update title by id (url untouched: updateBookmark takes title+url)
            const QString u = idx.data(Qt::UserRole + 3).toString();
            Database::instance()->updateBookmark(id, name, u);
            reload();
        }
    });
    QAction *del = m.addAction(Icons::themed("trash"), tr("Delete"));
    connect(del, &QAction::triggered, this, [this, id] {
        Database::instance()->removeBookmark(id);
        reload();
    });
    m.exec(m_tree->viewport()->mapToGlobal(pos));
}
