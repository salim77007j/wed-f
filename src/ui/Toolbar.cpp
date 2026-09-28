#include "Toolbar.h"
#include "AddressBar.h"
#include "AppSettings.h"
#include "BrowserTab.h"
#include "Database.h"
#include "DownloadManager.h"
#include "MainWindow.h"
#include "ThemeManager.h"
#include "TabWidget.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtWebEngineCore/QWebEngineHistory>

Toolbar::Toolbar(MainWindow *window, bool privateMode)
    : QToolBar(window), m_window(window), m_private(privateMode)
{
    setMovable(false);
    setIconSize(QSize(20, 20));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    buildUi();
    rebuildBookmarksBar();

    connect(DownloadManager::instance(), &DownloadManager::downloadsBadgeChanged,
            this, &Toolbar::setDownloadsBadge);
    connect(AppSettings::instance(), &AppSettings::bookmarksBarChanged, this, [this] {
        m_bookmarksBar->setVisible(AppSettings::instance()->showBookmarksBar());
    });
}

void Toolbar::buildUi()
{
    auto *w = new QWidget(this);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(6, 4, 6, 2);
    lay->setSpacing(2);

    // main row
    auto *row = new QHBoxLayout();
    row->setSpacing(2);

    m_back = new QToolButton(w);
    m_back->setIcon(Icons::themed("back"));
    m_back->setToolTip(tr("Back (Alt+←)"));
    m_back->setAutoRaise(true);
    m_back->setEnabled(false);
    connect(m_back, &QToolButton::clicked, m_window, [this] {
        if (auto *t = m_window->currentTab()) t->back();
    });
    connect(m_back, &QToolButton::pressed, this, [] {});
    // long-press → history dropdown via custom timer
    auto *backTimer = new QTimer(m_back);
    backTimer->setSingleShot(true);
    backTimer->setInterval(450);
    connect(m_back, &QToolButton::pressed, this, [backTimer] { backTimer->start(); });
    connect(m_back, &QToolButton::released, this, [backTimer] { backTimer->stop(); });
    connect(backTimer, &QTimer::timeout, this, &Toolbar::showBackHistory);
    row->addWidget(m_back);

    m_forward = new QToolButton(w);
    m_forward->setIcon(Icons::themed("forward"));
    m_forward->setToolTip(tr("Forward (Alt+→)"));
    m_forward->setAutoRaise(true);
    m_forward->setEnabled(false);
    connect(m_forward, &QToolButton::clicked, m_window, [this] {
        if (auto *t = m_window->currentTab()) t->forward();
    });
    auto *fwdTimer = new QTimer(m_forward);
    fwdTimer->setSingleShot(true);
    fwdTimer->setInterval(450);
    connect(m_forward, &QToolButton::pressed, this, [fwdTimer] { fwdTimer->start(); });
    connect(m_forward, &QToolButton::released, this, [fwdTimer] { fwdTimer->stop(); });
    connect(fwdTimer, &QTimer::timeout, this, &Toolbar::showForwardHistory);
    row->addWidget(m_forward);

    m_reloadStop = new QToolButton(w);
    m_reloadStop->setIcon(Icons::themed("reload"));
    m_reloadStop->setToolTip(tr("Reload (F5, Ctrl+R)"));
    m_reloadStop->setAutoRaise(true);
    connect(m_reloadStop, &QToolButton::clicked, this, [this] {
        if (!m_window->currentTab()) return;
        if (m_loading)
            m_window->currentTab()->stop();
        else
            m_window->currentTab()->reload();
    });
    row->addWidget(m_reloadStop);

    if (AppSettings::instance()->showHomeButton()) {
        m_home = new QToolButton(w);
        m_home->setIcon(Icons::themed("home"));
        m_home->setToolTip(tr("Home"));
        m_home->setAutoRaise(true);
        connect(m_home, &QToolButton::clicked, m_window, [this] {
            if (auto *t = m_window->currentTab())
                t->load(QUrl(AppSettings::instance()->homePage()));
        });
        row->addWidget(m_home);
    }

    // address bar
    m_address = new AddressBar(w);
    m_address->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_address->setFixedHeight(34);
    m_address->setStyleSheet(QStringLiteral(
        "AddressBar { background:%1; border:1px solid %2; border-radius:17px; }"
        "QLineEdit { background:transparent; border:none; font-size:14px; padding:2px 6px; color:%3; }"
        "QToolButton { background:transparent; border:none; }")
        .arg(ThemeManager::instance()->color(RoleInputBg).name(),
             ThemeManager::instance()->color(RoleBorder).name(),
             ThemeManager::instance()->color(RoleText).name()));
    row->addWidget(m_address, 1);

    if (m_private) {
        auto *privBadge = new QToolButton(w);
        privBadge->setIcon(Icons::themed("private"));
        privBadge->setToolTip(tr("Private window — browsing is not saved"));
        privBadge->setAutoRaise(true);
        privBadge->setEnabled(false);
        row->addWidget(privBadge);
    }

    m_downloads = new QToolButton(w);
    m_downloads->setIcon(Icons::themed("download"));
    m_downloads->setToolTip(tr("Downloads (Ctrl+J)"));
    m_downloads->setAutoRaise(true);
    connect(m_downloads, &QToolButton::clicked, m_window, &MainWindow::showDownloadsPanel);
    row->addWidget(m_downloads);

    m_menu = new QToolButton(w);
    m_menu->setIcon(Icons::themed("menu"));
    m_menu->setToolTip(tr("Menu"));
    m_menu->setAutoRaise(true);
    connect(m_menu, &QToolButton::clicked, this, &Toolbar::showMainMenu);
    row->addWidget(m_menu);

    lay->addLayout(row);

    // bookmarks bar row
    m_bookmarksBar = new QWidget(w);
    buildBookmarkBarUi();
    m_bookmarksBar->setVisible(AppSettings::instance()->showBookmarksBar());
    lay->addWidget(m_bookmarksBar);

    addWidget(w);
}

void Toolbar::buildBookmarkBarUi()
{
    if (QLayout *old = m_bookmarksBar->layout())
        delete old;
    auto *bl = new QHBoxLayout(m_bookmarksBar);
    bl->setContentsMargins(2, 1, 2, 1);
    bl->setSpacing(2);

    const auto bms = Database::instance()->bookmarks(0);
    for (const Database::BookmarkNode &n : bms) {
        if (n.folder || n.url.isEmpty()) continue;
        auto *btn = new QToolButton(m_bookmarksBar);
        btn->setText(n.title.isEmpty() ? QUrl(n.url).host() : n.title);
        btn->setIcon(Icons::themed("globe"));
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setAutoRaise(true);
        btn->setToolTip(n.url);
        btn->setStyleSheet(QStringLiteral("QToolButton { color:%1; padding:1px 8px; border-radius:6px; font-size:12px; }"
                                          "QToolButton:hover { background:%2; }")
                           .arg(ThemeManager::instance()->color(RoleText).name(),
                                ThemeManager::instance()->color(RoleChromeHover).name()));
        connect(btn, &QToolButton::clicked, m_window, [this, url = n.url] {
            m_window->tabWidget()->newTab(m_private, AppSettings::instance()->openLinksInForeground(), QUrl(url));
        });
        bl->addWidget(btn);
    }
    auto *stretch = new QWidget(m_bookmarksBar);
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bl->addWidget(stretch);
    m_bookmarksBar->setFixedHeight(28);
}

void Toolbar::rebuildBookmarksBar() { buildBookmarkBarUi(); }

void Toolbar::reloadIcons()
{
    m_back->setIcon(Icons::themed("back"));
    m_forward->setIcon(Icons::themed("forward"));
    m_reloadStop->setIcon(Icons::themed(m_loading ? "stop" : "reload"));
    if (m_home) m_home->setIcon(Icons::themed("home"));
    m_downloads->setIcon(Icons::themed("download"));
    m_menu->setIcon(Icons::themed("menu"));
    buildBookmarkBarUi();
}

void Toolbar::syncNavState(BrowserTab *tab)
{
    if (!tab) return;
    // loading state follows tab
    disconnect(m_loadConn);
    m_loadConn = connect(tab, &BrowserTab::loadStarted, this, [this] {
        m_loading = true;
        m_reloadStop->setIcon(Icons::themed("stop"));
        m_reloadStop->setToolTip(tr("Stop loading (Esc)"));
    });
    disconnect(m_loadEndConn);
    m_loadEndConn = connect(tab, &BrowserTab::loadFinished, this, [this](bool) {
        m_loading = false;
        m_reloadStop->setIcon(Icons::themed("reload"));
        m_reloadStop->setToolTip(tr("Reload (F5)"));
        updateNavButtons();
    });
    updateNavButtons();
}

void Toolbar::updateNavButtons()
{
    auto *t = m_window->currentTab();
    if (!t || t->isStartPage()) {
        m_back->setEnabled(false);
        m_forward->setEnabled(false);
        return;
    }
    m_back->setEnabled(t->page()->history()->canGoBack());
    m_forward->setEnabled(t->page()->history()->canGoForward());
}

void Toolbar::showBackHistory()
{
    auto *t = m_window->currentTab();
    if (!t || !t->page()->history()->canGoBack()) return;
    QMenu m(this);
    const int cur = t->page()->history()->currentItemIndex();
    const auto items = t->page()->history()->items();
    for (int i = cur - 1; i >= 0; --i) {
        QString title = items.at(i).title().isEmpty() ? items.at(i).url().toString() : items.at(i).title();
        if (title.length() > 46) title = title.left(43) + "...";
        QAction *a = m.addAction(Icons::themed("history"), title, this, [t, i] {
            // navigate back i times by triggering history navigation
            for (int k = 0; k < t->page()->history()->currentItemIndex() - i; ++k)
                t->page()->triggerAction(QWebEnginePage::Back);
        });
        Q_UNUSED(a);
    }
    m.exec(m_back->mapToGlobal(QPoint(0, m_back->height())));
}

void Toolbar::showForwardHistory()
{
    auto *t = m_window->currentTab();
    if (!t || !t->page()->history()->canGoForward()) return;
    QMenu m(this);
    const int cur = t->page()->history()->currentItemIndex();
    const auto items = t->page()->history()->items();
    for (int i = cur + 1; i < items.size(); ++i) {
        QString title = items.at(i).title().isEmpty() ? items.at(i).url().toString() : items.at(i).title();
        if (title.length() > 46) title = title.left(43) + "...";
        m.addAction(Icons::themed("history"), title, this, [t, i] {
            for (int k = 0; k < i - t->page()->history()->currentItemIndex(); ++k)
                t->page()->triggerAction(QWebEnginePage::Forward);
        });
    }
    m.exec(m_forward->mapToGlobal(QPoint(0, m_forward->height())));
}

void Toolbar::setDownloadsBadge(int active)
{
    m_dlBadge = active;
    m_downloads->setIcon(active > 0 ? Icons::make("download", ThemeManager::instance()->accent())
                                    : Icons::themed("download"));
    m_downloads->setToolTip(active > 0 ? tr("%1 downloads in progress").arg(active)
                                        : tr("Downloads (Ctrl+J)"));
}

void Toolbar::showMainMenu()
{
    QMenu m(this);
    auto add = [&m, this](const QString &text, const QIcon &icon, int id, const QString &shortcut = QString()) {
        QAction *a = m.addAction(icon, text, this, [this, id] { emit menuAction(id); });
        if (!shortcut.isEmpty()) a->setText(QString("%1\t%2").arg(text, shortcut));
        return a;
    };

    add(tr("New tab"), Icons::themed("plus"), ActNewTab, "Ctrl+T");
    add(tr("New window"), Icons::themed("window"), ActNewWindow, "Ctrl+N");
    add(tr("New private window"), Icons::themed("private"), ActNewPrivateWindow, "Ctrl+Shift+N");
    m.addSeparator();
    add(tr("Reopen closed tab"), Icons::themed("restore"), ActReopenClosed, "Ctrl+Shift+T");
    m.addSeparator();
    add(tr("Bookmarks"), Icons::themed("bookmark"), ActBookmarks, "Ctrl+B");
    add(tr("History"), Icons::themed("history"), ActHistory, "Ctrl+Y");
    add(tr("Downloads"), Icons::themed("download"), ActDownloads, "Ctrl+J");
    m.addSeparator();
    add(tr("Find in page…"), Icons::themed("find"), ActFind, "Ctrl+F");
    add(tr("Save page as…"), Icons::themed("download"), ActSavePage, "Ctrl+S");
    add(tr("Print…"), Icons::themed("print"), ActPrint, "Ctrl+P");
    m.addSeparator();
    add(tr("Zoom in"), Icons::themed("zoomin"), ActZoomIn, "Ctrl++");
    add(tr("Zoom out"), Icons::themed("zoomout"), ActZoomOut, "Ctrl+-");
    add(tr("Reset zoom"), Icons::themed("zoomreset"), ActZoomReset, "Ctrl+0");
    m.addSeparator();
    add(tr("Privacy dashboard"), Icons::themed("shield"), ActPrivacyDashboard);
    add(tr("Clear browsing data…"), Icons::themed("trash"), ActClearData, "Ctrl+Shift+Del");
    add(tr("Developer tools"), Icons::themed("devtools"), ActDevTools, "F12");
    m.addSeparator();
    add(tr("Settings"), Icons::themed("settings"), ActSettings);
    add(tr("About WED"), Icons::themed("info"), ActAbout);
    m.addSeparator();
    add(tr("Quit"), Icons::themed("close"), ActQuit, "Ctrl+Q");

    m.exec(m_menu->mapToGlobal(QPoint(0, m_menu->height())));
}
