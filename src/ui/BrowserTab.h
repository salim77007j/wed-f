#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTimer>

class WebPage;
class WebView;
class StartPage;

// A browser tab: start page phase → web phase. Owns WebView + WebPage,
// per-page blocked counters, zoom memory, crash state, audio state.
class BrowserTab : public QWidget
{
    Q_OBJECT
public:
    explicit BrowserTab(bool privateMode, QWidget *parent = nullptr);
    ~BrowserTab() override;

    void load(const QUrl &url);
    QUrl currentUrl() const;
    QString title() const;
    QIcon icon() const;
    WebView *view() const { return m_view; }
    WebPage *page() const { return m_page; }
    bool isStartPage() const { return m_onStart; }
    bool isCrashed() const { return m_crashed; }
    bool isAudible() const { return m_audible; }
    bool isMuted() const;
    void setMuted(bool m);
    bool isPinned() const { return m_pinned; }
    void setPinned(bool p);
    bool isPrivate() const { return m_private; }
    int loadProgress() const { return m_progress; }
    bool isLoading() const { return m_loading; }

    // per-page privacy counters (page-local, host-persistent in DB)
    int pageAdsBlocked() const { return m_adsBlocked; }
    int pageTrackersBlocked() const { return m_trackersBlocked; }
    int pageCookiesBlocked() const { return m_cookiesBlocked; }
    void setCookiesBlocked(int n) { m_cookiesBlocked = n; }

    // serialization for session restore
    QUrl url() const;
    QStringList backStackUrls() const;
    QStringList forwardStackUrls() const;
    int historyIndex() const;
    void restore(const QUrl &url, const QStringList &backUrls, const QStringList &forwardUrls);

    // lifecycle: freeze/discard for memory saving
    void freeze();
    void unfreeze();
    void discard();
    bool isFrozen() const;

    void setLastActiveTick(qint64 t) { m_lastActiveTick = t; }
    qint64 lastActiveTick() const { return m_lastActiveTick; }

    // navigation passthroughs
    void back() { if (m_page) m_page->triggerAction(QWebEnginePage::Back); }
    void forward() { if (m_page) m_page->triggerAction(QWebEnginePage::Forward); }
    void reload() { if (m_page) m_page->triggerAction(QWebEnginePage::Reload); }
    void reloadBypassCache() { if (m_page) m_page->triggerAction(QWebEnginePage::ReloadAndBypassCache); }
    void stop() { if (m_page) m_page->triggerAction(QWebEnginePage::Stop); }
    void focusView();

    // devtools page for this tab (created on demand, owned here)
    QWebEnginePage *ensureDevToolsPage();

    // window.open() handler plumbing (set by TabWidget)
    void setCreateViewHandler(std::function<QWebEngineView *(QWebEnginePage::WebWindowType)> h);

signals:
    void titleChanged(const QString &title);
    void urlChanged(const QUrl &url);
    void iconChanged(const QIcon &icon);
    void loadProgress(int progress);
    void loadFinished(bool ok);
    void loadStarted();
    void audibleChanged(bool audible);
    void closeRequested();
    void pageStatsChanged();
    void securityChanged();
    void crashedChanged(bool crashed);
    void zoomChanged(int percent);
    void openLinkRequested(const QUrl &url, int flags);
    void searchSelectionRequested(const QString &text);
    void devToolsRequested();
    void bookmarkLinkRequested(const QUrl &url, const QString &title);
    void windowOpenRequested(int windowType); // 3 = popup dialog
    void statusBarTextChanged(const QString &text);

private slots:
    void onLoadStarted();
    void onLoadFinished(bool ok);
    void onPrivacyBlocked(const QString &firstPartyUrl, const QString &host, int category);
    void applyCosmeticFilters();
    void onRenderTerminated(int status);

private:
    void buildUi();
    void switchToWeb();
    void switchToStart();

    QStackedWidget *m_stack = nullptr;
    StartPage *m_start = nullptr;
    WebView *m_view = nullptr;
    WebPage *m_page = nullptr;
    QWebEnginePage *m_devTools = nullptr;
    bool m_private;
    bool m_onStart = true;
    bool m_crashed = false;
    bool m_audible = false;
    bool m_pinned = false;
    bool m_loading = false;
    int m_progress = 0;
    int m_adsBlocked = 0;
    int m_trackersBlocked = 0;
    int m_cookiesBlocked = 0;
    qint64 m_lastActiveTick = 0;
    QString m_lastFirstParty;
    QUrl m_lastValidUrl;
    QUrl m_restoredUrl;
    QStringList m_pendingBack;
    QStringList m_pendingForward;
};
