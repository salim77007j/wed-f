#include "TabWidget.h"
#include "AppSettings.h"
#include "BrowserTab.h"
#include "Database.h"
#include "MainWindow.h"
#include "PrivacyEngine.h"
#include "ProfileCatalog.h"
#include "ThemeManager.h"
#include "WebPage.h"
#include "WebView.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

// ---------------------------------------------------------------- TabBar

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    setTabsClosable(true);
    setMovable(false); // custom drag handling
    setExpanding(false);
    setElideMode(Qt::ElideRight);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDocumentMode(false);
    setUsesScrollButtons(true);
    setMouseTracking(true);
}

QSize TabBar::tabSizeHint(int index) const
{
    const int pinned = tabData(index).toBool();
    const int minW = pinned ? 44 : 130;
    const int dynW = qBound(minW, qRound(width() / qMax(1.0, count() * 0.62)), 240);
    return QSize(pinned ? qMax(44, dynW / 3) : dynW, 34);
}

int TabBar::pinPx() const { return 0; }
void TabBar::mousePressEvent(QMouseEvent *e)
{
    m_pressPos = e->pos();
    m_pressIndex = tabAt(e->pos());
    m_dragging = false;
    m_moved = false;
    if (e->button() == Qt::LeftButton && m_pressIndex >= 0)
        m_dragging = true;
    QTabBar::mousePressEvent(e);
}

void TabBar::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->pos() - m_pressPos).manhattanLength() > 8) {
        m_moved = true;
        const int over = tabAt(e->pos());
        if (over >= 0 && over != m_pressIndex) {
            emit tabMoveRequested(m_pressIndex, over);
            m_pressIndex = over;
            return;
        }
    }
    QTabBar::mouseMoveEvent(e);
}

void TabBar::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_moved) {
        // detach: released outside tab bar area
        const QRect bar = rect();
        if (m_pressIndex >= 0 && !bar.contains(e->pos())) {
            emit tabDetachRequested(m_pressIndex, e->globalPosition().toPoint());
            m_dragging = false;
            return;
        }
    }
    m_dragging = false;
    QTabBar::mouseReleaseEvent(e);
}

void TabBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (tabAt(e->pos()) < 0) {
        emit emptyAreaDoubleClicked();
        return;
    }
    QTabBar::mouseDoubleClickEvent(e);
}

void TabBar::contextMenuEvent(QContextMenuEvent *e)
{
    const int idx = tabAt(e->pos());
    QMenu m(this);
    if (idx < 0) {
        QAction *newTab = m.addAction(Icons::themed("plus"), tr("New tab"));
        connect(newTab, &QAction::triggered, this, &TabBar::newTabShortcutRequested);
        m.exec(e->globalPos());
        return;
    }
    auto *tw = qobject_cast<TabWidget *>(parentWidget());
    if (!tw) return;
    const bool pinned = tabData(idx).toBool();

    QAction *reload = m.addAction(Icons::themed("reload"), tr("Reload"));
    connect(reload, &QAction::triggered, tw, [tw, idx] { tw->tabAt(idx)->reload(); });
    QAction *duplicate = m.addAction(tr("Duplicate tab"));
    connect(duplicate, &QAction::triggered, tw, [tw, idx] { tw->duplicateTab(idx); });
    QAction *pin = m.addAction(pinned ? tr("Unpin tab") : tr("Pin tab"));
    connect(pin, &QAction::triggered, tw, [tw, idx, pinned] { tw->setTabPinned(idx, !pinned); });
    QAction *mute = m.addAction(Icons::themed("mute"), tr("Mute site"));
    connect(mute, &QAction::triggered, tw, [tw, idx] { tw->toggleMuteTab(idx); });
    m.addSeparator();
    QAction *closeOthers = m.addAction(tr("Close other tabs"));
    connect(closeOthers, &QAction::triggered, tw, [tw, idx] { tw->closeOtherTabs(idx); });
    QAction *closeRight = m.addAction(tr("Close tabs to the right"));
    connect(closeRight, &QAction::triggered, tw, [tw, idx] { tw->closeTabsToRight(idx); });
    m.addSeparator();
    QAction *reopen = m.addAction(Icons::themed("restore"), tr("Reopen closed tab"));
    connect(reopen, &QAction::triggered, tw, &TabWidget::reopenClosedTab);
    QAction *close = m.addAction(tr("Close tab"));
    close->setShortcut(QKeySequence("Ctrl+W"));
    connect(close, &QAction::triggered, tw, [tw, idx] { tw->closeTab(idx); });
    m.exec(e->globalPos());
}

void TabBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = ThemeManager::instance()->isDark();
    const QColor baseBg = dark ? QColor("#202124") : QColor("#dee1e6");
    const QColor tabBg = dark ? QColor("#1a1b1e") : QColor("#c9ccd1");
    const QColor tabActiveBg = dark ? QColor("#35363a") : QColor("#ffffff");
    const QColor line = ThemeManager::instance()->accent();

    p.fillRect(rect(), baseBg);

    for (int i = 0; i < count(); ++i) {
        const QRect r = tabRect(i);
        if (r.isEmpty()) continue;
        const bool selected = i == currentIndex();
        auto *tw = qobject_cast<TabWidget *>(parentWidget());
        BrowserTab *bt = tw ? tw->tabAt(i) : nullptr;
        const bool audible = bt && bt->isAudible();
        const bool muted = bt && bt->isMuted();
        QPainterPath path;
        path.addRoundedRect(r.adjusted(1, 1, -1, 0), 8, 8);
        p.fillPath(path, selected ? tabActiveBg : tabBg);

        const bool pinned = tabData(i).toBool();
        if (selected)
            p.fillRect(QRect(r.left() + 6, r.bottom() - 2, r.width() - 12, 2), line);

        // icon (favicon / audio overlay)
        QRect iconRect(r.left() + 8, r.top() + 9, 16, 16);
        QIcon ic = tabIcon(i);
        if (!ic.isNull())
            p.drawPixmap(iconRect, ic.pixmap(16, 16));
        if (audible || muted) {
            QPixmap audioPm = Icons::pixmap(muted ? "mute" : "tab-audio",
                                            muted ? QColor("#d93025")
                                                  : ThemeManager::instance()->color(RoleTextSecondary), 16);
            p.drawPixmap(iconRect, audioPm);
        }

        // close button
        const int closeRight = r.right() - 22;
        if (!pinned && r.width() > 80) {
            QPixmap closePm = Icons::pixmap("close",
                                            ThemeManager::instance()->color(RoleTextSecondary), 14);
            p.drawPixmap(QPoint(closeRight, r.top() + 10), closePm);
        }

        // title
        if (!pinned && r.width() > 80) {
            p.setPen(QPen(selected ? (dark ? QColor("#e8eaed") : QColor("#202124"))
                                   : (dark ? QColor("#9aa0a6") : QColor("#5f6368"))));
            QFont f = p.font();
            f.setPixelSize(12);
            p.setFont(f);
            const int textLeft = iconRect.right() + 6;
            const int textWidth = closeRight - textLeft - 6;
            if (textWidth > 10) {
                QString text = p.fontMetrics().elidedText(tabText(i), Qt::ElideRight, textWidth);
                p.drawText(QRect(textLeft, r.top(), textWidth, r.height()), Qt::AlignVCenter, text);
            }
        }
    }
    p.end();
}

// ---------------------------------------------------------------- TabWidget

TabWidget::TabWidget(MainWindow *window, QWidget *parent)
    : QTabWidget(parent), m_window(window)
{
    auto *bar = new TabBar(this);
    setTabBar(bar);
    setDocumentMode(false);
    setContentsMargins(0, 0, 0, 0);
    connect(bar, &TabBar::tabMoveRequested, this, &TabWidget::moveTab);
    connect(bar, &TabBar::tabDetachRequested, this, &TabWidget::detachTab);
    connect(bar, &TabBar::emptyAreaDoubleClicked, this, [this] { newTab(); });
    connect(bar, &TabBar::newTabShortcutRequested, this, [this] { newTab(); });
    connect(bar, &QTabBar::tabCloseRequested, this, &TabWidget::onTabCloseRequested);
    connect(this, &QTabWidget::currentChanged, this, &TabWidget::onCurrentChanged);

    m_hibernateTimer.setInterval(30 * 1000);
    connect(&m_hibernateTimer, &QTimer::timeout, this, &TabWidget::hibernateCheck);
    m_hibernateTimer.start();
}

BrowserTab *TabWidget::newTab(bool privateMode, bool background, const QUrl &url)
{
    auto *tab = new BrowserTab(privateMode, this);
    wireTab(tab);
    const int idx = addTab(tab, tab->title());
    tabBar()->setTabData(idx, false);
    updateTabVisual(idx);

    // window.open plumbing
    tab->setCreateViewHandler([this, privateMode](QWebEnginePage::WebWindowType type) -> WebView * {
        return createViewForType(type, privateMode);
    });

    const QUrl target = url.isEmpty() ? QUrl("wed://start") : url;
    tab->load(target);
    if (!background)
        setCurrentIndex(idx);
    else
        tab->setLastActiveTick(0);
    emit tabCountChanged(count());
    return tab;
}

void TabWidget::wireTab(BrowserTab *tab)
{
    connect(tab, &BrowserTab::titleChanged, this, [this, tab](const QString &t) {
        for (int i = 0; i < count(); ++i)
            if (widget(i) == tab) {
                if (!tab->isPinned()) setTabText(i, t);
                updateTabVisual(i);
            }
    });
    connect(tab, &BrowserTab::iconChanged, this, [this, tab](const QIcon &ic) {
        for (int i = 0; i < count(); ++i)
            if (widget(i) == tab)
                setTabIcon(i, ic);
    });
    connect(tab, static_cast<void (BrowserTab::*)(int)>(&BrowserTab::loadProgress), this, [this, tab](int prog) {
        if (tab == currentTab())
            emit currentTabChanged(tab);
    });
    connect(tab, &BrowserTab::audibleChanged, this, [this, tab](bool audible) {
        Q_UNUSED(tab);
        // audible indicator handled by repaint via tabBar()->update()
        tabBar()->update();
    });
    connect(tab, &BrowserTab::closeRequested, this, [this, tab] {
        for (int i = 0; i < count(); ++i)
            if (widget(i) == tab) { closeTab(i); return; }
    });
    connect(tab, &BrowserTab::openLinkRequested, this,
            [this](const QUrl &url, int flags) {
        const bool bg = flags & int(WebView::OpenBackground);
        const bool priv = flags & int(WebView::OpenPrivate);
        newTab(priv, bg, url);
    });
    connect(tab, &BrowserTab::searchSelectionRequested, this, [this](const QString &text) {
        m_window->searchWeb(text);
    });
    connect(tab, &BrowserTab::devToolsRequested, this, [this, tab] {
        m_window->openDevToolsFor(tab);
    });
    connect(tab, &BrowserTab::bookmarkLinkRequested, this, [this](const QUrl &url, const QString &title) {
        m_window->addBookmarkWithDialog(url, title);
    });
    connect(tab, &BrowserTab::windowOpenRequested, this, [this](int type) {
        Q_UNUSED(type);
        // popup dialogs handled via createViewForType
    });
    connect(tab, &BrowserTab::crashedChanged, this, [this, tab](bool) {
        for (int i = 0; i < count(); ++i)
            if (widget(i) == tab) updateTabVisual(i);
    });
}

void TabWidget::updateTabVisual(int index)
{
    auto *tab = tabAt(index);
    if (!tab) return;
    setTabText(index, tab->title());
    setTabIcon(index, tab->icon());
    setTabToolTip(index, tab->currentUrl().toString());
    if (tab->isPrivate() && tabIcon(index).isNull())
        setTabIcon(index, Icons::themed("private"));
}

void TabWidget::onCurrentChanged(int index)
{
    auto *tab = tabAt(index);
    if (tab) {
        tab->setLastActiveTick(QDateTime::currentMSecsSinceEpoch());
        tab->unfreeze();
        tab->focusView();
        emit currentTabChanged(tab);
    }
    emit tabCountChanged(count());
}

void TabWidget::onTabCloseRequested(int index)
{
    closeTab(index);
}

void TabWidget::closeTab(int index)
{
    auto *tab = tabAt(index);
    if (!tab) return;
    const QUrl u = tab->url();
    const QString t = tab->title();
    Database::instance()->pushClosedTab(u.toString(), t);
    m_closedStack.push({u, t});
    if (m_closedStack.size() > 20) {
        // keep stack light; DB keeps longer list
        m_closedStack.remove(0);
    }
    removeTab(index);
    tab->deleteLater();
    if (count() == 0 && !m_shuttingDown)
        newTab();
    emit tabCountChanged(count());
    emit closedTabsAvailabilityChanged(!m_closedStack.isEmpty());
}

void TabWidget::closeOtherTabs(int index)
{
    for (int i = count() - 1; i >= 0; --i)
        if (i != index) closeTab(i);
}

void TabWidget::closeTabsToRight(int index)
{
    for (int i = count() - 1; i > index; --i)
        closeTab(i);
}

void TabWidget::duplicateTab(int index)
{
    auto *tab = tabAt(index);
    if (!tab) return;
    newTab(tab->isPrivate(), false, tab->currentUrl());
}

void TabWidget::reopenClosedTab()
{
    if (m_closedStack.isEmpty()) return;
    const auto pair = m_closedStack.pop();
    newTab(false, false, pair.first);
    emit closedTabsAvailabilityChanged(!m_closedStack.isEmpty());
}

void TabWidget::moveTab(int from, int to)
{
    QWidget *page = widget(from);
    const QIcon icon = tabIcon(from);
    const QString text = tabText(from);
    const QVariant data = tabBar()->tabData(from);
    removeTab(from);
    insertTab(to, page, icon, text);
    tabBar()->setTabData(to, data);
    setCurrentIndex(to);
}

void TabWidget::detachTab(int index, const QPoint &globalPos)
{
    auto *tab = tabAt(index);
    if (!tab) return;
    const QUrl url = tab->currentUrl();
    const bool priv = tab->isPrivate();
    removeTab(index);
    tab->deleteLater();
    // hand to window manager: new window at drop position with this url
    MainWindow::createDetached(m_window, priv, url, globalPos);
    if (count() == 0)
        newTab();
    emit tabCountChanged(count());
}

void TabWidget::setTabPinned(int index, bool pinned)
{
    tabBar()->setTabData(index, pinned);
    if (auto *tab = tabAt(index))
        tab->setPinned(pinned);
    setTabText(index, pinned ? QString() : tabText(index));
    tabBar()->update();
}

void TabWidget::toggleMuteTab(int index)
{
    auto *tab = tabAt(index);
    if (!tab) return;
    tab->setMuted(!tab->isMuted());
    tabBar()->update();
}

void TabWidget::nextTab()
{
    setCurrentIndex((currentIndex() + 1) % count());
}

void TabWidget::prevTab()
{
    setCurrentIndex((currentIndex() - 1 + count()) % count());
}

void TabWidget::selectTab(int index)
{
    if (index < 0 || index >= count()) return;
    setCurrentIndex(index);
}

BrowserTab *TabWidget::tabAt(int i) const
{
    return qobject_cast<BrowserTab *>(widget(i));
}

BrowserTab *TabWidget::currentTab() const
{
    return tabAt(currentIndex());
}

QUrl TabWidget::currentTabUrl() const
{
    auto *t = currentTab();
    return t ? t->currentUrl() : QUrl();
}

// ---------------------------------------------------------------- popups

WebView *TabWidget::createViewForType(QWebEnginePage::WebWindowType type, bool privateMode)
{
    if (type == QWebEnginePage::WebDialog) {
        // real popup window: small frameless window with a web view
        auto *popup = new QWidget(nullptr, Qt::Window);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setWindowFlags(popup->windowFlags() | Qt::Dialog);
        auto *lay = new QVBoxLayout(popup);
        lay->setContentsMargins(0, 0, 0, 0);
        auto *view = new WebView(popup);
        QWebEngineProfile *profile = privateMode ? ProfileCatalog::instance()->incognito()
                                                 : ProfileCatalog::instance()->persistent();
        auto *page = new WebPage(profile, view);
        view->setPage(page);
        lay->addWidget(view);
        popup->resize(480, 560);
        popup->show();
        // wire the new page's createWindow handler recursively
        return view;
    }
    // new tab (foreground as Chromium default for _blank without features)
    BrowserTab *tab = newTab(privateMode, false);
    return tab->view();
}

// ---------------------------------------------------------------- hibernate

void TabWidget::hibernateCheck()
{
    const int minutes = AppSettings::instance()->hibernateMinutes();
    if (minutes <= 0) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < count(); ++i) {
        auto *tab = tabAt(i);
        if (!tab || i == currentIndex()) continue;
        const qint64 idleMs = now - tab->lastActiveTick();
        if (idleMs > qint64(minutes) * 60000)
            tab->freeze();
    }
}

// ---------------------------------------------------------------- session

QList<TabWidget::TabSession> TabWidget::serializeTabs() const
{
    QList<TabSession> out;
    for (int i = 0; i < count(); ++i) {
        auto *tab = tabAt(i);
        if (!tab) continue;
        TabSession s;
        s.url = tab->url();
        s.pinned = tab->isPinned();
        s.muted = tab->isMuted();
        s.back = tab->backStackUrls();
        s.forward = tab->forwardStackUrls();
        out.append(s);
    }
    return out;
}

int TabWidget::currentIndexSerialized() const { return currentIndex(); }

void TabWidget::restoreSession(const QList<TabSession> &tabs, int currentIndex)
{
    setUpdatesEnabled(false);
    for (const TabSession &s : tabs) {
        BrowserTab *tab = newTab(false, true);
        tab->restore(s.url, s.back, s.forward);
        if (s.pinned) setTabPinned(indexOf(tab), true);
        if (s.muted) tab->setMuted(true);
    }
    if (currentIndex >= 0 && currentIndex < count())
        setCurrentIndex(currentIndex);
    setUpdatesEnabled(true);
}
