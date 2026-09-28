#include "BrowserTab.h"
#include "AppSettings.h"
#include "Database.h"
#include "PrivacyEngine.h"
#include "ProfileCatalog.h"
#include "StartPage.h"
#include "ThemeManager.h"
#include "WebPage.h"
#include "WebView.h"

#include <QApplication>
#include <QtWebEngineCore/QWebEngineHistory>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVBoxLayout>

BrowserTab::BrowserTab(bool privateMode, QWidget *parent)
    : QWidget(parent), m_private(privateMode)
{
    buildUi();
    m_lastActiveTick = QDateTime::currentMSecsSinceEpoch();
}

BrowserTab::~BrowserTab()
{
    if (m_devTools) {
        m_devTools->deleteLater();
    }
}

void BrowserTab::buildUi()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    lay->addWidget(m_stack);

    QWebEngineProfile *profile = m_private ? ProfileCatalog::instance()->incognito()
                                           : ProfileCatalog::instance()->persistent();

    m_start = new StartPage(this);
    connect(m_start, &StartPage::navigateRequested, this, [this](const QUrl &u) {
        load(u);
    });
    m_stack->addWidget(m_start);

    m_view = new WebView(this);
    m_page = new WebPage(profile, m_view);
    m_view->setPage(m_page);
    m_stack->addWidget(m_view);

    m_stack->setCurrentWidget(m_start);

    // page → tab signals
    connect(m_page, &QWebEnginePage::titleChanged, this, [this](const QString &t) {
        // ignore blank titles from pending loads
        emit titleChanged(t);
    });
    connect(m_page, &QWebEnginePage::urlChanged, this, [this](const QUrl &u) {
        emit urlChanged(u);
        if (u.toString() != "about:blank")
            switchToWeb();
    });
    connect(m_page, &QWebEnginePage::iconChanged, this, &BrowserTab::iconChanged);
    connect(m_page, &QWebEnginePage::loadStarted, this, &BrowserTab::onLoadStarted);
    connect(m_page, &QWebEnginePage::loadProgress, this, [this](int p) {
        m_progress = p;
        emit loadProgress(p);
    });
    connect(m_page, &QWebEnginePage::loadFinished, this, &BrowserTab::onLoadFinished);
    connect(m_page, &QWebEnginePage::recentlyAudibleChanged, this, [this](bool a) {
        m_audible = a;
        emit audibleChanged(a);
    });
    connect(m_page, &QWebEnginePage::linkHovered, this, &BrowserTab::statusBarTextChanged);
    connect(m_page, &QWebEnginePage::windowCloseRequested, this, &BrowserTab::closeRequested);
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus, int code) {
        onRenderTerminated(code);
    });
    connect(m_page, &WebPage::pageSecurityChanged, this, &BrowserTab::securityChanged);

    // view → tab signals
    connect(m_view, &WebView::zoomChanged, this, &BrowserTab::zoomChanged);
    connect(m_view, &WebView::openLinkRequested, this,
            [this](const QUrl &url, WebView::OpenFlags f) {
        emit openLinkRequested(url, int(f));
    });
    connect(m_view, &WebView::searchSelectionRequested, this, &BrowserTab::searchSelectionRequested);
    connect(m_view, &WebView::devToolsRequested, this, &BrowserTab::devToolsRequested);
    connect(m_view, &WebView::bookmarkLinkRequested, this, &BrowserTab::bookmarkLinkRequested);

    // per-page privacy stats
    connect(PrivacyEngine::instance(), &PrivacyEngine::blocked, this,
            &BrowserTab::onPrivacyBlocked, Qt::QueuedConnection);
}

void BrowserTab::setCreateViewHandler(std::function<QWebEngineView *(QWebEnginePage::WebWindowType)> h)
{
    m_view->setCreateViewHandler(h);
    m_page->setCreateWindowHandler([this, h](QWebEnginePage::WebWindowType type) -> QWebEnginePage * {
        QWebEngineView *v = h(type);
        if (!v) return nullptr;
        QWebEnginePage *p = v->page();
        return p;
    });
}

void BrowserTab::load(const QUrl &url)
{
    m_crashed = false;
    emit crashedChanged(false);
    if (url.toString() == "wed://start" || url.toString().isEmpty()) {
        switchToStart();
        m_start->refresh();
        emit urlChanged(QUrl("wed://start"));
        emit titleChanged(m_private ? tr("Private tab") : tr("New tab"));
        emit loadFinished(true);
        return;
    }
    switchToWeb();
    m_page->resetCertError();
    m_view->setZoomFromSiteMemory(url);
    m_view->load(url);
}

void BrowserTab::switchToWeb()
{
    if (m_stack->currentWidget() != m_view) {
        m_stack->setCurrentWidget(m_view);
        m_onStart = false;
    }
}

void BrowserTab::switchToStart()
{
    if (m_stack->currentWidget() != m_start) {
        m_stack->setCurrentWidget(m_start);
        m_onStart = true;
    }
}

QUrl BrowserTab::currentUrl() const
{
    if (m_onStart) return QUrl("wed://start");
    const QUrl u = m_page->url();
    return u;
}

QUrl BrowserTab::url() const
{
    if (m_onStart) return QUrl("wed://start");
    const QUrl u = m_page->url();
    if (u.toString().isEmpty() || u.toString() == "about:blank")
        return m_lastValidUrl;
    return u;
}

QString BrowserTab::title() const
{
    if (m_onStart)
        return m_private ? tr("Private tab") : tr("New tab");
    const QString t = m_page->title();
    if (t.isEmpty())
        return currentUrl().host().isEmpty() ? tr("Loading…") : currentUrl().host();
    return t;
}

QIcon BrowserTab::icon() const
{
    if (m_onStart) return {};
    QIcon i = m_page->icon();
    if (i.isNull()) i = Icons::themed("globe");
    return i;
}

bool BrowserTab::isMuted() const { return m_page ? m_page->isAudioMuted() : false; }

void BrowserTab::setMuted(bool m)
{
    if (m_page) m_page->setAudioMuted(m);
}

void BrowserTab::setPinned(bool p)
{
    m_pinned = p;
    emit titleChanged(title());
}

void BrowserTab::focusView()
{
    if (m_onStart)
        m_start->focusSearch();
    else
        m_view->setFocus();
}

void BrowserTab::onLoadStarted()
{
    m_loading = true;
    m_progress = 0;
    m_adsBlocked = 0;
    m_trackersBlocked = 0;
    emit loadStarted();
    emit pageStatsChanged();
}

void BrowserTab::onLoadFinished(bool ok)
{
    m_loading = false;
    m_progress = 100;
    const QUrl u = m_page->url();
    if (ok && !u.toString().isEmpty() && u.toString() != "about:blank") {
        m_lastValidUrl = u;
        if (!m_private)
            Database::instance()->addHistory(u.toString(), m_page->title());
        applyCosmeticFilters();
    }
    emit loadFinished(ok);
    emit pageStatsChanged();
}

void BrowserTab::onPrivacyBlocked(const QString &firstPartyUrl, const QString &host, int category)
{
    const QUrl fp(firstPartyUrl);
    const QUrl mine = currentUrl();
    if (mine.isEmpty() || fp.host() != mine.host())
        return;
    if (category == PrivacyEngine::BlockedAd) m_adsBlocked++;
    else m_trackersBlocked++;
    emit pageStatsChanged();
}

void BrowserTab::applyCosmeticFilters()
{
    const QString host = Utils::hostOfUrl(m_page->url());
    if (host.isEmpty()) return;
    if (!AppSettings::instance()->blockAds() && !AppSettings::instance()->blockTrackers()) return;
    if (!PrivacyEngine::instance()->shieldsEnabledFor(host)) return;

    const QString json = PrivacyEngine::instance()->cosmeticFor(host);
    if (json.isEmpty() || json == "{\"generic\":[],\"specific\":[]}") return;

    // inject a stylesheet via JS — runs in the page's main world
    const QString js = QStringLiteral(R"(
(function() {
  try {
    var data = %1;
    var all = (data.generic || []).concat(data.specific || []);
    if (!all.length) return;
    var css = all.join(',\n') + ' { display: none !important; visibility: hidden !important; }';
    var st = document.getElementById('wed-cosmetic') || document.createElement('style');
    st.id = 'wed-cosmetic';
    st.textContent = css;
    (document.head || document.documentElement).appendChild(st);
  } catch (e) {}
})();
)").arg(json);
    m_page->runJavaScript(js);
}

void BrowserTab::onRenderTerminated(int status)
{
    Q_UNUSED(status);
    m_crashed = true;
    emit crashedChanged(true);
    emit titleChanged(title());
}

// ---------------------------------------------------------------- lifecycle

void BrowserTab::freeze()
{
    if (m_page && !m_onStart)
        m_page->setLifecycleState(QWebEnginePage::LifecycleState::Frozen);
}

void BrowserTab::unfreeze()
{
    if (m_page && !m_onStart)
        m_page->setLifecycleState(QWebEnginePage::LifecycleState::Active);
}

void BrowserTab::discard()
{
    if (m_page && !m_onStart)
        m_page->setLifecycleState(QWebEnginePage::LifecycleState::Discarded);
}

bool BrowserTab::isFrozen() const
{
    if (!m_page || m_onStart) return false;
    return m_page->lifecycleState() == QWebEnginePage::LifecycleState::Frozen
           || m_page->lifecycleState() == QWebEnginePage::LifecycleState::Discarded;
}

// ---------------------------------------------------------------- session

QStringList BrowserTab::backStackUrls() const
{
    QStringList out;
    if (m_onStart || !m_page) return out;
    const int idx = m_page->history()->currentItemIndex();
    const auto items = m_page->history()->items();
    for (int i = 0; i < idx; ++i)
        out << items.at(i).url().toString();
    return out;
}

QStringList BrowserTab::forwardStackUrls() const
{
    QStringList out;
    if (m_onStart || !m_page) return out;
    const int idx = m_page->history()->currentItemIndex();
    const auto items = m_page->history()->items();
    for (int i = idx + 1; i < items.size(); ++i)
        out << items.at(i).url().toString();
    return out;
}

int BrowserTab::historyIndex() const
{
    if (m_onStart || !m_page) return -1;
    return m_page->history()->currentItemIndex();
}

void BrowserTab::restore(const QUrl &u, const QStringList &backUrls, const QStringList &forwardUrls)
{
    m_pendingBack = backUrls;
    m_pendingForward = forwardUrls;
    load(u);
}

QWebEnginePage *BrowserTab::ensureDevToolsPage()
{
    if (!m_devTools) {
        QWebEngineProfile *profile = m_private ? ProfileCatalog::instance()->incognito()
                                               : ProfileCatalog::instance()->persistent();
        m_devTools = new QWebEnginePage(profile, this);
        m_page->setDevToolsPage(m_devTools);
    }
    return m_devTools;
}
