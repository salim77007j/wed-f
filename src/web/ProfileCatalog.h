#pragma once
#include <QObject>
#include <QtWebEngineCore/QWebEngineProfile>

class RequestInterceptor;
class CookieJar;

// Owns the two browser profiles (persistent + private), attaches the request
// interceptor, cookie policy, fingerprint-protection user script, UA overrides.
class ProfileCatalog : public QObject
{
    Q_OBJECT
public:
    static ProfileCatalog *instance();

    QWebEngineProfile *persistent() const { return m_persistent; }
    QWebEngineProfile *incognito() const { return m_incognito; }

    // privacy wiring for both profiles (idempotent)
    void attachAll();

    // live settings application
    void applyUserAgent();
    void applyFingerprintScript();
    void reloadPrivacyPolicies();   // cookie policy, etc.

    static QString storageRoot();

private:
    explicit ProfileCatalog(QObject *parent = nullptr);
    QWebEngineProfile *m_persistent = nullptr;
    QWebEngineProfile *m_incognito = nullptr;
    RequestInterceptor *m_interceptor = nullptr;
    CookieJar *m_cookieJarPersistent = nullptr;
    CookieJar *m_cookieJarIncognito = nullptr;
    bool m_attached = false;
};
