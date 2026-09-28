#include "MainWindow.h"
#include "AddressBar.h"
#include "AppSettings.h"
#include "BrowserTab.h"
#include "Database.h"
#include "FindBar.h"
#include "Panels.h"
#include "PrivacyDashboard.h"
#include "PrivacyEngine.h"
#include "SessionManager.h"
#include "SettingsDialog.h"
#include "TabWidget.h"
#include "ThemeManager.h"
#include "Toolbar.h"
#include "Utils.h"
#include "ProfileCatalog.h"
#include "WebView.h"

#include <QApplication>
#include <QCloseEvent>
#include <QClipboard>
#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPainter>
#include <QWidgetAction>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegularExpression>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTemporaryFile>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWebEngineView>

static const char *kMenuStyleKey = "menu_style";

MainWindow::MainWindow(bool privateMode, const QStringList &urls, QWidget *parent)
    : QMainWindow(parent), m_private(privateMode)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(":/icons/app.png")); // may be missing; icons drawn at runtime
    buildUi();
    wireShortcuts();

    // session autosave
    m_sessionTimer.setInterval(30000);
    connect(&m_sessionTimer, &QTimer::timeout, this, &MainWindow::saveSession);
    if (!m_private)
        m_sessionTimer.start();

    // startup
    if (urls.isEmpty()) {
        applyStartupState();
    } else {
        for (const QString &u : urls)
            m_tabs->newTab(m_private, false, QUrl::fromUserInput(u));
    }
    updateWindowTitles();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    // toolbar + tabs + statusbar
    m_toolbar = new Toolbar(this, m_private);
    addToolBar(m_toolbar);
    setContextMenuPolicy(Qt::NoContextMenu);

    m_tabs = new TabWidget(this, this);
    setCentralWidget(m_tabs);
    connect(m_tabs, &TabWidget::currentTabChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs, &TabWidget::downloadBadgeForNewWindow, this, [this](int) {});

    buildDocks();
    buildStatusBar();
    buildMenus();

    m_findBar = new FindBar(this);
    m_findBar->hide();

    setMinimumSize(560, 420);
    resize(1280, 820);
    setUnifiedTitleAndToolBarOnMac(false);

    // toolbar ↔ window wiring
    connect(m_toolbar->addressBar(), &AddressBar::navigateRequested, this, [this](const QUrl &u) {
        if (auto *t = currentTab()) t->load(u);
    });
    connect(m_toolbar->addressBar(), &AddressBar::searchRequested, this, &MainWindow::searchWeb);
    connect(m_toolbar->addressBar(), &AddressBar::shieldClicked, this, &MainWindow::onShieldPopup);
    connect(m_toolbar->addressBar(), &AddressBar::siteInfoRequested, this, &MainWindow::onSiteInfoPopup);
    connect(m_toolbar->addressBar(), &AddressBar::starClicked, this, [this] {
        if (auto *t = currentTab())
            addBookmarkWithDialog(t->currentUrl(), t->title());
    });
    connect(m_toolbar, &Toolbar::menuAction, this, [this](int id) {
        // dispatched by Toolbar's menu; large action ids live here
        switch (id) {
        case Toolbar::ActNewTab: m_tabs->newTab(m_private); break;
        case Toolbar::ActNewWindow: emit newWindowRequested(false); break;
        case Toolbar::ActNewPrivateWindow: openPrivateWindow(); break;
        case Toolbar::ActReopenClosed: m_tabs->reopenClosedTab(); break;
        case Toolbar::ActHistory: showHistoryPanel(); break;
        case Toolbar::ActDownloads: showDownloadsPanel(); break;
        case Toolbar::ActBookmarks: showBookmarksPanel(); break;
        case Toolbar::ActBookmarksManager: showBookmarksPanel(); break;
        case Toolbar::ActAddBookmark: addBookmarkWithDialog(currentTab()->currentUrl(), currentTab()->title()); break;
        case Toolbar::ActFind: toggleFindBar(); break;
        case Toolbar::ActPrint: printPage(); break;
        case Toolbar::ActSavePage: savePageAs(); break;
        case Toolbar::ActSettings: showSettings(); break;
        case Toolbar::ActPrivacyDashboard: showPrivacyDashboard(); break;
        case Toolbar::ActClearData: showClearBrowsingData(); break;
        case Toolbar::ActDevTools: openDevToolsFor(currentTab()); break;
        case Toolbar::ActZoomIn: zoomDelta(1); break;
        case Toolbar::ActZoomOut: zoomDelta(-1); break;
        case Toolbar::ActZoomReset: zoomReset(); break;
        case Toolbar::ActAbout: showAbout(); break;
        case Toolbar::ActQuit: close(); break;
        default: break;
        }
    });
}

void MainWindow::buildDocks()
{
    m_downloadsPanel = new DownloadsPanel(this);
    m_downloadsDock = new QDockWidget(tr("Downloads"), this);
    m_downloadsDock->setWidget(m_downloadsPanel);
    m_downloadsDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_downloadsDock->hide();
    addDockWidget(Qt::BottomDockWidgetArea, m_downloadsDock);

    m_bookmarksPanel = new BookmarksPanel(this);
    m_bookmarksDock = new QDockWidget(tr("Bookmarks"), this);
    m_bookmarksDock->setWidget(m_bookmarksPanel);
    m_bookmarksDock->hide();
    addDockWidget(Qt::LeftDockWidgetArea, m_bookmarksDock);
    connect(m_bookmarksPanel, &BookmarksPanel::openUrlRequested, this, [this](const QUrl &u) {
        m_tabs->newTab(m_private, false, u);
    });

    m_historyPanel = new HistoryPanel(this);
    m_historyDock = new QDockWidget(tr("History"), this);
    m_historyDock->setWidget(m_historyPanel);
    m_historyDock->hide();
    addDockWidget(Qt::LeftDockWidgetArea, m_historyDock);
    connect(m_historyPanel, &HistoryPanel::openUrlRequested, this, [this](const QUrl &u, bool bg) {
        m_tabs->newTab(m_private, bg, u);
    });
    tabifyDockWidget(m_bookmarksDock, m_historyDock);
}

void MainWindow::buildStatusBar()
{
    auto *sb = statusBar();
    sb->setFixedHeight(26);
    m_statusText = new QLabel(sb);
    m_statusText->setMinimumWidth(200);
    sb->addWidget(m_statusText);
    QWidget *stretch = new QWidget(sb);
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sb->addPermanentWidget(stretch);

    m_statusShield = new QToolButton(sb);
    m_statusShield->setIcon(Icons::themed("shield"));
    m_statusShield->setToolTip(tr("Protections"));
    m_statusShield->setAutoRaise(true);
    connect(m_statusShield, &QToolButton::clicked, this, &MainWindow::onShieldPopup);
    sb->addPermanentWidget(m_statusShield);

    m_statusStats = new QToolButton(sb);
    m_statusStats->setText(tr("0 ads · 0 trackers"));
    m_statusStats->setToolTip(tr("Blocked on this page"));
    m_statusStats->setAutoRaise(true);
    connect(m_statusStats, &QToolButton::clicked, this, &MainWindow::onShieldPopup);
    sb->addPermanentWidget(m_statusStats);

    m_statusZoom = new QToolButton(sb);
    m_statusZoom->setText("100%");
    m_statusZoom->setToolTip(tr("Zoom — click to reset"));
    m_statusZoom->setAutoRaise(true);
    connect(m_statusZoom, &QToolButton::clicked, this, &MainWindow::zoomReset);
    sb->addPermanentWidget(m_statusZoom);
}

void MainWindow::buildMenus()
{
    // menu bar (native, real)
    QMenuBar *mb = menuBar();
    QMenu *file = mb->addMenu(tr("&File"));
    file->addAction(tr("New &tab"), QKeySequence("Ctrl+T"), this, [this] { m_tabs->newTab(m_private); });
    file->addAction(tr("New &window"), QKeySequence("Ctrl+N"), this, [this] { emit newWindowRequested(false); });
    file->addAction(tr("New &private window"), QKeySequence("Ctrl+Shift+N"), this, [this] { openPrivateWindow(); });
    file->addSeparator();
    file->addAction(tr("&Open file…"), QKeySequence("Ctrl+O"), this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("Open file"), QString(),
            "Web pages (*.html *.htm *.svg *.png *.jpg *.pdf);;All files (*)");
        if (!f.isEmpty())
            m_tabs->newTab(m_private, false, QUrl::fromLocalFile(f));
    });
    file->addAction(tr("&Save page as…"), QKeySequence("Ctrl+S"), this, &MainWindow::savePageAs);
    file->addAction(tr("&Print…"), QKeySequence("Ctrl+P"), this, &MainWindow::printPage);
    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"), QKeySequence("Ctrl+Q"), this, &MainWindow::close);

    QMenu *edit = mb->addMenu(tr("&Edit"));
    edit->addAction(tr("&Undo"), QKeySequence::Undo, this, [this] { if (currentTab()) currentTab()->page()->triggerAction(QWebEnginePage::Undo); });
    edit->addAction(tr("&Redo"), QKeySequence::Redo, this, [this] { if (currentTab()) currentTab()->page()->triggerAction(QWebEnginePage::Redo); });
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), QKeySequence::Cut, this, [this] { if (currentTab()) currentTab()->page()->triggerAction(QWebEnginePage::Cut); });
    edit->addAction(tr("&Copy"), QKeySequence::Copy, this, [this] { if (currentTab()) currentTab()->page()->triggerAction(QWebEnginePage::Copy); });
    edit->addAction(tr("&Paste"), QKeySequence::Paste, this, [this] { if (currentTab()) currentTab()->page()->triggerAction(QWebEnginePage::Paste); });
    edit->addSeparator();
    edit->addAction(tr("&Find…"), QKeySequence("Ctrl+F"), this, &MainWindow::toggleFindBar);

    QMenu *view = mb->addMenu(tr("&View"));
    QAction *showBm = view->addAction(tr("&Bookmarks bar"), QKeySequence("Ctrl+Shift+B"), this, [this] {
        AppSettings::instance()->setShowBookmarksBar(!AppSettings::instance()->showBookmarksBar());
        m_toolbar->rebuildBookmarksBar();
    });
    showBm->setCheckable(true);
    showBm->setChecked(AppSettings::instance()->showBookmarksBar());
    view->addSeparator();
    view->addAction(tr("Zoom &in"), QKeySequence("Ctrl++"), this, [this] { zoomDelta(1); });
    view->addAction(tr("Zoom &out"), QKeySequence("Ctrl+-"), this, [this] { zoomDelta(-1); });
    view->addAction(tr("Zoom &reset"), QKeySequence("Ctrl+0"), this, &MainWindow::zoomReset);
    view->addSeparator();
    view->addAction(tr("&Downloads panel"), QKeySequence("Ctrl+Shift+Y"), this, &MainWindow::showDownloadsPanel);
    view->addAction(tr("&History panel"), QKeySequence("Ctrl+Y"), this, &MainWindow::showHistoryPanel);
    view->addAction(tr("&Full screen"), QKeySequence("F11"), this, [this] {
        isFullScreen() ? showNormal() : showFullScreen();
    });

    QMenu *hist = mb->addMenu(tr("&History"));
    hist->addAction(tr("&Back"), QKeySequence("Alt+Left"), this, [this] { if (currentTab()) currentTab()->back(); });
    hist->addAction(tr("&Forward"), QKeySequence("Alt+Right"), this, [this] { if (currentTab()) currentTab()->forward(); });
    hist->addSeparator();
    hist->addAction(tr("Reopen closed &tab"), QKeySequence("Ctrl+Shift+T"), this, [this] { m_tabs->reopenClosedTab(); });
    hist->addSeparator();
    // recent history quick entries
    for (const Database::HistoryEntry &e : Database::instance()->recentHistory(10)) {
        QString t = e.title.isEmpty() ? e.url : e.title;
        if (t.length() > 40) t = t.left(37) + "...";
        hist->addAction(t, this, [this, url = e.url] { m_tabs->newTab(m_private, false, QUrl(url)); });
    }
    hist->addSeparator();
    hist->addAction(tr("Show all &history"), this, &MainWindow::showHistoryPanel);

    QMenu *book = mb->addMenu(tr("&Bookmarks"));
    book->addAction(tr("Bookmark this &page"), QKeySequence("Ctrl+D"), this, [this] {
        addBookmarkWithDialog(currentTab()->currentUrl(), currentTab()->title());
    });
    book->addAction(tr("Show &bookmarks"), QKeySequence("Ctrl+Shift+O"), this, &MainWindow::showBookmarksPanel);
    book->addSeparator();
    // bookmark quick entries (top-level folder)
    for (const Database::BookmarkNode &n : Database::instance()->bookmarks(0)) {
        if (n.folder || n.url.isEmpty()) continue;
        QString t = n.title.isEmpty() ? n.url : n.title;
        if (t.length() > 40) t = t.left(37) + "...";
        book->addAction(Icons::themed("star"), t, this, [this, url = n.url] {
            m_tabs->newTab(m_private, false, QUrl(url));
        });
    }

    QMenu *tools = mb->addMenu(tr("&Tools"));
    tools->addAction(tr("&Downloads"), QKeySequence("Ctrl+J"), this, &MainWindow::showDownloadsPanel);
    tools->addAction(tr("&Privacy dashboard"), this, &MainWindow::showPrivacyDashboard);
    tools->addAction(tr("&Clear browsing data…"), QKeySequence("Ctrl+Shift+Del"), this, &MainWindow::showClearBrowsingData);
    tools->addAction(tr("&Developer tools"), QKeySequence("F12"), this, [this] { openDevToolsFor(currentTab()); });
    tools->addSeparator();
    tools->addAction(tr("&Settings…"), this, [this] { showSettings(); });

    QMenu *help = mb->addMenu(tr("&Help"));
    help->addAction(tr("&About WED"), this, &MainWindow::showAbout);
    Q_UNUSED(quit);
}

void MainWindow::wireShortcuts()
{
    auto sc = [this](const char *seq, auto fn) {
        auto *s = new QShortcut(QKeySequence(seq), this);
        connect(s, &QShortcut::activated, this, fn);
    };
    sc("Ctrl+L", [this] { focusAddressBar(); });
    sc("Alt+D", [this] { focusAddressBar(); });
    sc("F6", [this] { focusAddressBar(); });
    sc("Ctrl+W", [this] { m_tabs->closeTab(m_tabs->currentIndex()); });
    sc("Ctrl+Tab", [this] { m_tabs->nextTab(); });
    sc("Ctrl+Shift+Tab", [this] { m_tabs->prevTab(); });
    sc("F5", [this] { if (currentTab()) currentTab()->reload(); });
    sc("Ctrl+R", [this] { if (currentTab()) currentTab()->reload(); });
    sc("Ctrl+Shift+R", [this] { if (currentTab()) currentTab()->reloadBypassCache(); });
    sc("Esc", [this] { if (currentTab()) currentTab()->stop(); });
    sc("Alt+Home", [this] { if (currentTab()) currentTab()->load(QUrl("wed://start")); });
    for (int i = 1; i <= 8; ++i)
        sc(QString("Ctrl+%1").arg(i).toUtf8().constData(), [this, i] { m_tabs->selectTab(i - 1); });
    sc("Ctrl+9", [this] { m_tabs->selectTab(m_tabs->count() - 1); });
    // Ctrl+B toggles bookmarks panel
    sc("Ctrl+B", [this] { showBookmarksPanel(); });
}

void MainWindow::applyStartupState()
{
    AppSettings *s = AppSettings::instance();
    if (m_private) {
        m_tabs->newTab(true);
        return;
    }
    const int mode = s->startupMode();
    if (mode == AppSettings::StartupUrls) {
        const QStringList urls = s->startupUrls();
        if (urls.isEmpty()) { m_tabs->newTab(); return; }
        for (const QString &u : urls)
            m_tabs->newTab(false, false, QUrl::fromUserInput(u));
    } else if (mode == AppSettings::StartupContinue) {
        // restore last session if available
        if (SessionManager::instance()->hasLastSession()) {
            const SessionManager::SessionData d = SessionManager::instance()->loadLastSession();
            if (!d.tabs.isEmpty()) {
                m_tabs->restoreSession(d.tabs, d.currentIndex);
                return;
            }
        }
        m_tabs->newTab();
    } else {
        m_tabs->newTab();
    }
}

void MainWindow::onCurrentTabChanged(BrowserTab *tab)
{
    if (!tab) return;
    // wire dynamic state
    m_toolbar->addressBar()->setTab(tab);
    m_toolbar->syncNavState(tab);
    onPageStatsChanged();
    updateWindowTitles();

    disconnect(m_urlChangedConn);
    disconnect(m_zoomConn);
    disconnect(m_statusConn);
    m_urlChangedConn = connect(tab, &BrowserTab::urlChanged, this, [this](const QUrl &) {
        m_toolbar->addressBar()->updateDisplayForTab();
        updateWindowTitles();
        onPageStatsChanged();
    });
    m_zoomConn = connect(tab, &BrowserTab::zoomChanged, this, [this](int z) {
        m_zoomCurrent = z;
        m_statusZoom->setText(QString::number(z) + "%");
    });
    m_statusConn = connect(tab, &BrowserTab::statusBarTextChanged, this, [this](const QString &t) {
        m_statusText->setText(t);
    });
    connect(tab, &BrowserTab::pageStatsChanged, this, &MainWindow::onPageStatsChanged);
    m_zoomCurrent = qRound(tab->view()->zoomFactor() * 100);
    m_statusZoom->setText(QString::number(m_zoomCurrent) + "%");
}

void MainWindow::onPageStatsChanged()
{
    auto *t = currentTab();
    if (!t) return;
    const QString host = Utils::hostOfUrl(t->currentUrl());
    const bool off = !PrivacyEngine::instance()->shieldsEnabledFor(host) && !host.isEmpty();
    const int ads = off ? -1 : t->pageAdsBlocked();
    const int trackers = t->pageTrackersBlocked();
    m_toolbar->addressBar()->setShieldCount(ads, trackers, t->pageCookiesBlocked());
    if (!off)
        m_statusStats->setText(tr("%1 ads · %2 trackers").arg(t->pageAdsBlocked()).arg(trackers));
    else
        m_statusStats->setText(tr("Shields off"));
}

void MainWindow::onDownloadsBadge(int active)
{
    m_toolbar->setDownloadsBadge(active);
}

void MainWindow::updateWindowTitles()
{
    auto *t = currentTab();
    QString title = t ? t->title() : QString();
    if (title.isEmpty()) title = tr("New tab");
    setWindowTitle(m_private ? tr("%1 — WED Private").arg(title)
                             : tr("%1 — WED Browser").arg(title));
}

AddressBar *MainWindow::addressBar() const { return m_toolbar->addressBar(); }

BrowserTab *MainWindow::currentTab() const { return m_tabs->currentTab(); }

void MainWindow::navigateCurrent(const QUrl &url)
{
    if (auto *t = currentTab())
        t->load(url);
}

void MainWindow::searchWeb(const QString &text)
{
    if (text.trimmed().isEmpty()) return;
    navigateCurrent(QUrl(AddressBar::searchUrlFor(text)));
}

void MainWindow::focusAddressBar()
{
    m_toolbar->addressBar()->focusAndSelectAll();
}

void MainWindow::zoomDelta(int steps)
{
    if (auto *t = currentTab())
        t->view()->applyZoomDelta(steps);
}

void MainWindow::zoomReset()
{
    if (auto *t = currentTab())
        t->view()->resetZoom();
}

void MainWindow::toggleFindBar()
{
    m_findBar->attachTo(currentTab());
    m_findBar->setVisible(!m_findBar->isVisible());
    if (m_findBar->isVisible())
        m_findBar->setFocus();
}

void MainWindow::openDevToolsFor(BrowserTab *tab)
{
    if (!tab) return;
    QWebEnginePage *dev = tab->ensureDevToolsPage();
    if (!dev) return;
    // host the devtools page in a docked view (real Chromium DevTools)
    auto *dock = new QDockWidget(tr("Developer tools — %1").arg(tab->title()), this);
    auto *view = new QWebEngineView(dock);
    view->setPage(dev);
    dock->setWidget(view);
    dock->setAttribute(Qt::WA_DeleteOnClose);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->show();
    connect(dock, &QObject::destroyed, this, [tab] {
        if (tab->page())
            tab->page()->setDevToolsPage(nullptr);
    });
}

void MainWindow::addBookmarkWithDialog(const QUrl &url, const QString &title)
{
    if (url.isEmpty() || url.toString() == "wed://start") return;
    Database *db = Database::instance();
    if (db->isBookmarked(url.toString())) {
        // edit
        bool ok = false;
        const QString newTitle = QInputDialog::getText(this, tr("Edit bookmark"),
            tr("Name:"), QLineEdit::Normal, db->bookmarkTitle(url.toString()), &ok);
        if (ok && !newTitle.isEmpty()) {
            db->updateBookmarkByUrl(url.toString(), newTitle);
            m_toolbar->rebuildBookmarksBar();
            m_bookmarksPanel->reload();
        }
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add bookmark"),
        tr("Name:"), QLineEdit::Normal, title.isEmpty() ? url.host() : title, &ok);
    if (ok) {
        db->addBookmark(0, name, url.toString());
        m_toolbar->rebuildBookmarksBar();
        m_bookmarksPanel->reload();
    }
}

void MainWindow::savePageAs()
{
    auto *t = currentTab();
    if (!t || t->isStartPage()) return;
    const QString suggested = t->title().simplified().replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_") + ".html";
    const QString target = QFileDialog::getSaveFileName(this, tr("Save page as"),
        AppSettings::instance()->downloadDir() + "/" + suggested, "HTML (*.html);;PDF (*.pdf)");
    if (target.isEmpty()) return;
    if (target.endsWith(".pdf", Qt::CaseInsensitive))
        t->page()->saveAsPdf(target);
    else
        t->page()->saveHtml(target);
}

void MainWindow::printPage()
{
    auto *t = currentTab();
    if (!t || t->isStartPage()) return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // print via PDF serialization then print through Qt (Chromium has no direct dialog printing in Qt)
    QTemporaryFile tmp(QDir::tempPath() + "/wed-print-XXXXXX.pdf");
    if (!tmp.open()) return;
    const QString pdfPath = tmp.fileName();
    connect(t->page(), &QWebEnginePage::pdfPrintingFinished, this,
            &MainWindow::printFromPdf, Qt::SingleShotConnection);
    m_pendingPrinter = &printer;
    t->page()->printToPdf(pdfPath, printer.pageLayout());
}

void MainWindow::printFromPdf(const QString &path, bool success)
{
    if (!success || !m_pendingPrinter) return;
    // rasterize pdf pages and send to printer
    // (Qt can't print PDF directly; this keeps the feature real using poppler-free approach)
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray data = f.readAll();
    f.close();
    // write to a temp file for QPdfDocument
    QTemporaryFile tmpPdf(QDir::tempPath() + "/wed-print-XXXXXX.pdf");
    tmpPdf.setAutoRemove(false);
    if (!tmpPdf.open()) return;
    tmpPdf.write(data);
    tmpPdf.close();

    QPdfDocument doc;
    if (doc.load(tmpPdf.fileName()) != QPdfDocument::Error::None) return;
    QPainter painter(m_pendingPrinter);
    for (int i = 0; i < doc.pageCount(); ++i) {
        if (i > 0) m_pendingPrinter->newPage();
        const QSize target = m_pendingPrinter->pageLayout()
                                 .paintRectPixels(m_pendingPrinter->resolution())
                                 .size();
        QImage img = doc.render(i, target);
        if (!img.isNull())
            painter.drawImage(QRect(QPoint(0, 0), target), img);
    }
    painter.end();
    QFile::remove(tmpPdf.fileName());
}

void MainWindow::openPrivateWindow() { emit newWindowRequested(true); }

void MainWindow::showPrivacyDashboard()
{
    PrivacyDashboard dlg(this);
    dlg.exec();
    onPageStatsChanged();
}

void MainWindow::showSettings(int page)
{
    SettingsDialog dlg(this);
    if (page > 0) dlg.setCurrentPage(page);
    dlg.exec();
    m_toolbar->rebuildBookmarksBar();
    m_toolbar->reloadIcons();
    onPageStatsChanged();
}

void MainWindow::showClearBrowsingData()
{
    ClearBrowsingDataDialog dlg(this);
    dlg.exec();
    m_historyPanel->reload();
    m_bookmarksPanel->reload();
    onPageStatsChanged();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About WED"),
        tr("<b>WED Browser %1</b><br><br>"
           "A fast, private, native web browser.<br><br>"
           "Engine: Chromium (via Qt WebEngine %2)<br>"
           "Privacy core: Rust %3<br>"
           "UI: Qt %4<br><br>"
           "%5")
        .arg(AppSettings::versionString(),
             QString::fromLatin1(qVersion()),
             QStringLiteral("wed-core 1.0"),
             QString::fromLatin1(qVersion()),
             "Built with care."));
}

void MainWindow::showDownloadsPanel()
{
    m_downloadsDock->show();
    m_downloadsDock->raise();
    m_downloadsPanel->reload();
}

void MainWindow::showBookmarksPanel()
{
    if (m_bookmarksDock->isVisible()) { m_bookmarksDock->hide(); return; }
    m_bookmarksDock->show();
    m_bookmarksDock->raise();
    m_bookmarksPanel->reload();
}

void MainWindow::showHistoryPanel()
{
    if (m_historyDock->isVisible()) { m_historyDock->hide(); return; }
    m_historyDock->show();
    m_historyDock->raise();
    m_historyPanel->reload();
}

void MainWindow::saveSession()
{
    if (m_private || m_shuttingDown) return;
    SessionManager::instance()->save(m_tabs->serializeTabs(), m_tabs->currentIndexSerialized());
}

void MainWindow::onShieldPopup()
{
    auto *t = currentTab();
    if (!t) return;
    const QString host = Utils::hostOfUrl(t->currentUrl());
    if (host.isEmpty()) return;

    const Database::SiteStats st = Database::instance()->totalsFor(host);
    const bool off = !PrivacyEngine::instance()->shieldsEnabledFor(host);

    QMenu m(this);
    QLabel *header = new QLabel(tr("<b>Protections for %1</b>").arg(host), &m);
    header->setContentsMargins(10, 8, 10, 4);
    auto *wa = new QWidgetAction(&m);
    wa->setDefaultWidget(header);
    wa->setEnabled(false);
    m.addAction(wa);
    QLabel *body = new QLabel(tr("Blocked on this page: %1 ads, %2 trackers<br>"
                                 "Blocked all-time here: %3 ads, %4 trackers")
                              .arg(t->pageAdsBlocked()).arg(t->pageTrackersBlocked())
                              .arg(st.ads).arg(st.trackers), &m);
    body->setContentsMargins(10, 0, 10, 8);
    auto *wb = new QWidgetAction(&m);
    wb->setDefaultWidget(body);
    wb->setEnabled(false);
    m.addAction(wb);
    QAction *toggle = m.addAction(off ? tr("Enable protections for this site")
                                      : tr("Turn off protections for this site"));
    connect(toggle, &QAction::triggered, this, [this, host, off] {
        PrivacyEngine::instance()->setShieldException(host, !off);
        // reload page to apply
        if (auto *t2 = currentTab()) t2->reload();
    });
    m.addSeparator();
    QAction *dash = m.addAction(tr("Open privacy dashboard"));
    connect(dash, &QAction::triggered, this, &MainWindow::showPrivacyDashboard);
    m.exec(m_statusShield->mapToGlobal(QPoint(0, -m.sizeHint().height())));
}

void MainWindow::onSiteInfoPopup()
{
    auto *t = currentTab();
    if (!t) return;
    const QUrl u = t->currentUrl();
    QMenu m(this);
    QLabel *header = new QLabel(tr("<b>%1</b><br><small>%2</small>").arg(u.host().isEmpty() ? tr("This page") : u.host(), u.toString().toHtmlEscaped()), &m);
    header->setTextFormat(Qt::RichText);
    header->setContentsMargins(10, 8, 10, 8);
    auto *wh = new QWidgetAction(&m);
    wh->setDefaultWidget(header);
    wh->setEnabled(false);
    m.addAction(wh);
    m.addSeparator();
    QAction *permissions = m.addAction(tr("Site settings…"));
    connect(permissions, &QAction::triggered, this, [this] { showSettings(SettingsDialog::PagePermissions); });
    QAction *cookies = m.addAction(tr("Cookies and site data…"));
    connect(cookies, &QAction::triggered, this, [this] { showSettings(SettingsDialog::PagePrivacy); });
    m.exec(QCursor::pos());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_shuttingDown = true;
    m_tabs->setShuttingDown(true);
    if (!m_private) {
        saveSession();
        SessionManager::instance()->markCleanExit();
    }
    // clear-on-exit privacy options (real)
    AppSettings *s = AppSettings::instance();
    if (s->clearHistoryOnExit()) Database::instance()->clearHistory();
    if (s->clearCookiesOnExit()) {
        ProfileCatalog::instance()->persistent()->cookieStore()->deleteAllCookies();
    }
    if (s->clearCacheOnExit()) {
        ProfileCatalog::instance()->persistent()->clearHttpCache();
    }
    QMainWindow::closeEvent(event);
    emit lastWindowClosed();
}

MainWindow *MainWindow::createDetached(MainWindow *origin, bool privateMode, const QUrl &url, const QPoint &pos)
{
    auto *w = new MainWindow(privateMode, QStringList{url.toString()});
    if (pos != QPoint(-1, -1))
        w->move(pos);
    w->show();
    return w;
}
