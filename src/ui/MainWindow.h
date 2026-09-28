#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QToolButton>
#include <QTimer>

class TabWidget;
class AddressBar;
class Toolbar;
class FindBar;
class BrowserTab;
class QStatusBar;
class DownloadsPanel;
class BookmarksPanel;
class HistoryPanel;

// The main browser window: toolbar, tab widget, status bar, find bar overlay,
// panel docks, menu, shortcuts, session save, crash-safe shutdown.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool privateMode, const QStringList &urls, QWidget *parent = nullptr);
    ~MainWindow() override;

    static MainWindow *createDetached(MainWindow *origin, bool privateMode, const QUrl &url, const QPoint &pos);
    bool isPrivate() const { return m_private; }
    TabWidget *tabWidget() const { return m_tabs; }
    AddressBar *addressBar() const;
    Toolbar *toolbar() const { return m_toolbar; }

    // actions invoked from anywhere
    void searchWeb(const QString &text);
    void openDevToolsFor(BrowserTab *tab);
    void addBookmarkWithDialog(const QUrl &url, const QString &title);
    void toggleFindBar();
    void showPrivacyDashboard();
    void showSettings(int page = 0);
    void showClearBrowsingData();
    void showAbout();
    void showDownloadsPanel();
    void showBookmarksPanel();
    void showHistoryPanel();
    void zoomDelta(int steps);
    void zoomReset();
    void savePageAs();
    void printPage();
    void openPrivateWindow();
    void focusAddressBar();
    BrowserTab *currentTab() const;
    void navigateCurrent(const QUrl &url);

signals:
    void newWindowRequested(bool privateMode);
    void lastWindowClosed();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onCurrentTabChanged(BrowserTab *tab);
    void onPageStatsChanged();
    void onDownloadsBadge(int active);
    void saveSession();
    void onShieldPopup();
    void onSiteInfoPopup();
    void printFromPdf(const QString &path, bool success);

private:
    void buildUi();
    void buildMenus();
    void buildStatusBar();
    void buildDocks();
    void wireShortcuts();
    void applyStartupState();
    void updateWindowTitles();

    bool m_private;
    TabWidget *m_tabs = nullptr;
    Toolbar *m_toolbar = nullptr;
    FindBar *m_findBar = nullptr;
    QLabel *m_statusText = nullptr;
    QToolButton *m_statusZoom = nullptr;
    QToolButton *m_statusShield = nullptr;
    QToolButton *m_statusStats = nullptr;
    DownloadsPanel *m_downloadsPanel = nullptr;
    BookmarksPanel *m_bookmarksPanel = nullptr;
    HistoryPanel *m_historyPanel = nullptr;
    QDockWidget *m_downloadsDock = nullptr;
    QDockWidget *m_bookmarksDock = nullptr;
    QDockWidget *m_historyDock = nullptr;
    QTimer m_sessionTimer;
    QMetaObject::Connection m_urlChangedConn;
    QMetaObject::Connection m_zoomConn;
    QMetaObject::Connection m_statusConn;
    QPrinter *m_pendingPrinter = nullptr;
    int m_zoomCurrent = 100;
    bool m_shuttingDown = false;
};
