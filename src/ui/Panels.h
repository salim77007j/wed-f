#pragma once
#include <QWidget>
#include <QStandardItemModel>
#include <QClipboard>
#include <QApplication>
#include <QTableView>
#include <QTreeView>

class HistoryTableModel;
class DownloadsModel;
class MainWindow;

// Downloads panel: live table, pause/resume/cancel, open/show-in-folder.
class DownloadsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadsPanel(MainWindow *window, QWidget *parent = nullptr);
    void reload();
private slots:
    void onCustomMenu(const QPoint &pos);
    void onDoubleClicked(const QModelIndex &idx);
private:
    MainWindow *m_window;
    DownloadsModel *m_model = nullptr;
    QTableView *m_table = nullptr;
};

// History panel: grouped table + search + per-row open/delete.
class HistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPanel(MainWindow *window, QWidget *parent = nullptr);
    void reload();
signals:
    void openUrlRequested(const QUrl &url, bool background);
private slots:
    void onDoubleClicked(const QModelIndex &idx);
    void onCustomMenu(const QPoint &pos);
    void onSearchChanged(const QString &text);
private:
    MainWindow *m_window;
    HistoryTableModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLineEdit *m_search = nullptr;
};

// Bookmarks panel: tree of folders + bookmarks, add/edit/delete via context menu,
// import/export JSON.
class BookmarksPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BookmarksPanel(MainWindow *window, QWidget *parent = nullptr);
    void reload();
signals:
    void openUrlRequested(const QUrl &url);
private slots:
    void onItemActivated(const QModelIndex &idx);
    void onCustomMenu(const QPoint &pos);
private:
    void buildTree();
    MainWindow *m_window;
    QTreeView *m_tree = nullptr;
    QStandardItemModel *m_model = nullptr;
};
