#pragma once
#include <QToolBar>
#include <QToolButton>

class AddressBar;
class BrowserTab;
class MainWindow;

// Navigation toolbar: back/forward (with history dropdown), reload/stop, home,
// address bar, downloads button (badge), main menu. Plus bookmarks bar row.
class Toolbar : public QToolBar
{
    Q_OBJECT
public:
    // action ids dispatched through menuAction signal
    enum MenuAction {
        ActNewTab = 1, ActNewWindow, ActNewPrivateWindow, ActReopenClosed,
        ActHistory, ActDownloads, ActBookmarks, ActBookmarksManager,
        ActAddBookmark, ActFind, ActPrint, ActSavePage, ActSettings,
        ActPrivacyDashboard, ActClearData, ActDevTools, ActZoomIn, ActZoomOut,
        ActZoomReset, ActAbout, ActQuit,
    };

    explicit Toolbar(MainWindow *window, bool privateMode);
    AddressBar *addressBar() const { return m_address; }
    void syncNavState(BrowserTab *tab);
    void setDownloadsBadge(int active);
    void rebuildBookmarksBar();
    void reloadIcons();

signals:
    void menuAction(int id);

private slots:
    void showBackHistory();
    void showForwardHistory();
    void showMainMenu();
    void updateNavButtons();

private:
    void buildUi();
    void buildBookmarkBarUi();

    MainWindow *m_window;
    bool m_private;
    AddressBar *m_address = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_forward = nullptr;
    QToolButton *m_reloadStop = nullptr;
    QToolButton *m_home = nullptr;
    QToolButton *m_downloads = nullptr;
    QToolButton *m_menu = nullptr;
    QWidget *m_bookmarksBar = nullptr;
    QMetaObject::Connection m_loadConn;
    QMetaObject::Connection m_loadEndConn;
    bool m_loading = false;
    int m_dlBadge = 0;
};
