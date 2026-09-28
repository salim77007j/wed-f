#pragma once
#include <QObject>
#include <QSettings>
#include <QStringList>

// Application-wide settings. All keys map to real behavior in the app.
// INI-backed, change notifications via signals, live-applied wherever possible.
class AppSettings : public QObject
{
    Q_OBJECT
public:
    enum ThemeMode { ThemeSystem = 0, ThemeLight = 1, ThemeDark = 2 };
    enum StartupMode { StartupNewTab = 0, StartupContinue = 1, StartupUrls = 2 };
    enum CookiePolicy { CookiesAllowAll = 0, CookiesBlockThirdParty = 1, CookiesBlockAll = 2 };
    enum TabPosition { TabsTop = 0, TabsBottom = 1, TabsLeft = 2 };
    enum WebRtcPolicy { WebRtcDefault = 0, WebRtcPublicOnly = 1, WebRtcDisable = 2 };
    enum ProxyMode { ProxySystem = 0, ProxyNone = 1, ProxyManual = 2 };

    static AppSettings *instance();

    // general
    int startupMode() const { return m_s.value("general/startupMode", 0).toInt(); }
    QStringList startupUrls() const { return m_s.value("general/startupUrls").toStringList(); }
    QString homePage() const { return m_s.value("general/homePage", "wed://start").toString(); }
    bool showHomeButton() const { return m_s.value("general/showHomeButton", true).toBool(); }
    QString downloadDir() const;
    bool askWhereToSave() const { return m_s.value("general/askWhereToSave", false).toBool(); }
    bool openLinksInForeground() const { return m_s.value("general/openLinksForeground", false).toBool(); }
    int hibernateMinutes() const { return m_s.value("general/hibernateMinutes", 20).toInt(); }

    // appearance
    int themeMode() const { return m_s.value("appearance/theme", 0).toInt(); }
    QString accentColor() const { return m_s.value("appearance/accent", "#0fa2a0").toString(); }
    int tabPosition() const { return m_s.value("appearance/tabPosition", 0).toInt(); }
    bool showBookmarksBar() const { return m_s.value("appearance/showBookmarksBar", true).toBool(); }
    int defaultZoom() const { return m_s.value("appearance/defaultZoom", 100).toInt(); }
    int minFontSize() const { return m_s.value("appearance/minFontSize", 0).toInt(); }
    QString uiFont() const { return m_s.value("appearance/uiFont").toString(); }

    // search
    QString searchEngine() const { return m_s.value("search/engine", "duckduckgo").toString(); }
    bool searchSuggestions() const { return m_s.value("search/suggestions", false).toBool(); }

    // privacy
    bool blockAds() const { return m_s.value("privacy/blockAds", true).toBool(); }
    bool blockTrackers() const { return m_s.value("privacy/blockTrackers", true).toBool(); }
    int cookiePolicy() const { return m_s.value("privacy/cookiePolicy", 1).toInt(); }
    bool dntHeader() const { return m_s.value("privacy/dnt", true).toBool(); }
    bool gpcHeader() const { return m_s.value("privacy/gpc", true).toBool(); }
    bool fingerprintProtection() const { return m_s.value("privacy/fingerprint", true).toBool(); }
    bool httpsFirst() const { return m_s.value("privacy/httpsFirst", true).toBool(); }
    int webrtcPolicy() const { return m_s.value("privacy/webrtc", 1).toInt(); }
    bool clearHistoryOnExit() const { return m_s.value("privacy/clearHistoryOnExit", false).toBool(); }
    bool clearCookiesOnExit() const { return m_s.value("privacy/clearCookiesOnExit", false).toBool(); }
    bool clearCacheOnExit() const { return m_s.value("privacy/clearCacheOnExit", false).toBool(); }
    bool easyListEnabled() const { return m_s.value("privacy/listEasyList", true).toBool(); }
    bool easyPrivacyEnabled() const { return m_s.value("privacy/listEasyPrivacy", true).toBool(); }

    // security / permissions defaults: 0 ask, 1 allow, 2 block
    int permissionDefault(const QString &feature) const
    { return m_s.value("security/default_" + feature, 0).toInt(); }

    // advanced
    bool hardwareAcceleration() const { return m_s.value("advanced/hwAccel", true).toBool(); }
    int userAgentMode() const { return m_s.value("advanced/userAgent", 0).toInt(); }
    QString customUserAgent() const { return m_s.value("advanced/customUA").toString(); }
    int proxyMode() const { return m_s.value("advanced/proxyMode", 0).toInt(); }
    QString proxyHost() const { return m_s.value("advanced/proxyHost").toString(); }
    int proxyPort() const { return m_s.value("advanced/proxyPort", 8080).toInt(); }

    // setters (emit changed signals)
    void setStartupMode(int v) { set("general/startupMode", v); }
    void setStartupUrls(const QStringList &v) { set("general/startupUrls", v); }
    void setHomePage(const QString &v) { set("general/homePage", v); }
    void setShowHomeButton(bool v) { set("general/showHomeButton", v); }
    void setDownloadDir(const QString &v) { set("general/downloadDir", v); }
    void setAskWhereToSave(bool v) { set("general/askWhereToSave", v); }
    void setOpenLinksInForeground(bool v) { set("general/openLinksForeground", v); }
    void setHibernateMinutes(int v) { set("general/hibernateMinutes", v); }
    void setThemeMode(int v) { set("appearance/theme", v); }
    void setAccentColor(const QString &v) { set("appearance/accent", v); }
    void setTabPosition(int v) { set("appearance/tabPosition", v); }
    void setShowBookmarksBar(bool v) { set("appearance/showBookmarksBar", v); }
    void setDefaultZoom(int v) { set("appearance/defaultZoom", v); }
    void setMinFontSize(int v) { set("appearance/minFontSize", v); }
    void setUiFont(const QString &v) { set("appearance/uiFont", v); }
    void setSearchEngine(const QString &v) { set("search/engine", v); }
    void setSearchSuggestions(bool v) { set("search/suggestions", v); }
    void setBlockAds(bool v) { set("privacy/blockAds", v); emit privacyChanged(); }
    void setBlockTrackers(bool v) { set("privacy/blockTrackers", v); emit privacyChanged(); }
    void setCookiePolicy(int v) { set("privacy/cookiePolicy", v); emit privacyChanged(); }
    void setDntHeader(bool v) { set("privacy/dnt", v); emit privacyChanged(); }
    void setGpcHeader(bool v) { set("privacy/gpc", v); emit privacyChanged(); }
    void setFingerprintProtection(bool v) { set("privacy/fingerprint", v); emit privacyChanged(); }
    void setHttpsFirst(bool v) { set("privacy/httpsFirst", v); emit privacyChanged(); }
    void setWebRtcPolicy(int v) { set("privacy/webrtc", v); }
    void setClearHistoryOnExit(bool v) { set("privacy/clearHistoryOnExit", v); }
    void setClearCookiesOnExit(bool v) { set("privacy/clearCookiesOnExit", v); }
    void setClearCacheOnExit(bool v) { set("privacy/clearCacheOnExit", v); }
    void setEasyListEnabled(bool v) { set("privacy/listEasyList", v); emit filtersChanged(); }
    void setEasyPrivacyEnabled(bool v) { set("privacy/listEasyPrivacy", v); emit filtersChanged(); }
    void setPermissionDefault(const QString &f, int v) { set("security/default_" + f, v); }
    void setHardwareAcceleration(bool v) { set("advanced/hwAccel", v); }
    void setUserAgentMode(int v) { set("advanced/userAgent", v); emit userAgentChanged(); }
    void setCustomUserAgent(const QString &v) { set("advanced/customUA", v); emit userAgentChanged(); }
    void setProxyMode(int v) { set("advanced/proxyMode", v); }
    void setProxyHost(const QString &v) { set("advanced/proxyHost", v); }
    void setProxyPort(int v) { set("advanced/proxyPort", v); }

    void sync() { m_s.sync(); }

    // search engines registry
    struct SearchEngineInfo { QString id, name, searchUrl, suggestionUrl; };
    static QList<SearchEngineInfo> searchEngines();

    // browser version
    static QString versionString() { return QStringLiteral("1.0.0"); }
    static QString fullVersionString();

signals:
    void themeChanged();
    void privacyChanged();
    void filtersChanged();
    void userAgentChanged();
    void searchEngineChanged();
    void bookmarksBarChanged();

private:
    explicit AppSettings(QObject *parent = nullptr);
    void set(const QString &key, const QVariant &value)
    {
        if (m_s.value(key) == value) return;
        m_s.setValue(key, value);
        if (key.startsWith("appearance/")) emit themeChanged();
        if (key.startsWith("search/")) emit searchEngineChanged();
        if (key == "appearance/showBookmarksBar") emit bookmarksBarChanged();
    }
    QSettings m_s;
};

// Utilities shared across modules
namespace Utils {
QString hostOfUrl(const QUrl &url);
QString registrableDomain(const QString &host);
QString formatBytes(qint64 bytes);
}
