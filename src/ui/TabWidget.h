#pragma once
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QStack>

class BrowserTab;
class MainWindow;

// Tab bar with: close-on-hover buttons, pinned tabs, audio indicators,
// drag reordering, detach-to-window (drag outside bar), context menu.
class TabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit TabBar(QWidget *parent = nullptr);
    int pinPx() const;
signals:
    void tabMoveRequested(int from, int to);
    void tabDetachRequested(int index, const QPoint &globalDropPos);
    void emptyAreaDoubleClicked();
    void newTabShortcutRequested();
protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    QSize tabSizeHint(int index) const override;
private:
    QPoint m_pressPos;
    int m_pressIndex = -1;
    bool m_dragging = false;
    bool m_moved = false;
};

// Container for BrowserTabs: creation/removal/duplicate/reopen, switching,
// hibernation policy, per-tab context menu actions.
class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit TabWidget(MainWindow *window, QWidget *parent = nullptr);

    BrowserTab *newTab(bool privateMode = false, bool background = false, const QUrl &url = QUrl());
    BrowserTab *tabAt(int i) const;
    BrowserTab *currentTab() const;
    void closeTab(int index);
    void closeOtherTabs(int index);
    void closeTabsToRight(int index);
    void duplicateTab(int index);
    void reopenClosedTab();
    void moveTab(int from, int to);
    void detachTab(int index, const QPoint &globalPos);
    void setTabPinned(int index, bool pinned);
    void toggleMuteTab(int index);
    void nextTab();
    void prevTab();
    void selectTab(int index);

    void setShuttingDown(bool v) { m_shuttingDown = v; }

    // session serialization
    struct TabSession { QUrl url; bool pinned; bool muted; QStringList back, forward; };
    QList<TabSession> serializeTabs() const;
    int currentIndexSerialized() const;
    void restoreSession(const QList<TabSession> &tabs, int currentIndex);

    // window.open / popup handling
    WebView *createViewForType(QWebEnginePage::WebWindowType type, bool privateMode);
    QUrl currentTabUrl() const;

signals:
    void currentTabChanged(BrowserTab *tab);
    void tabCountChanged(int count);
    void closedTabsAvailabilityChanged(bool available);
    void downloadBadgeForNewWindow(int count);

private slots:
    void onCurrentChanged(int index);
    void onTabCloseRequested(int index);
    void hibernateCheck();

private:
    void wireTab(BrowserTab *tab);
    void updateTabVisual(int index);
    MainWindow *m_window;
    QStack<QPair<QUrl, QString>> m_closedStack;
    QTimer m_hibernateTimer;
    bool m_shuttingDown = false;
};
