#pragma once
#include <QObject>
#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QNetworkCookie>
#include <atomic>

// Intercepts every outgoing request: blocks ads/trackers via the Rust engine,
// attaches DNT / GPC privacy headers, enforces HTTPS-first main frames.
class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit RequestInterceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
};

// Third-party cookie control + session cleanup using QWebEngineCookieStore.
class CookieJar : public QObject
{
    Q_OBJECT
public:
    explicit CookieJar(QWebEngineProfile *profile, QObject *parent = nullptr);
    void applyPolicy();
    int cookiesBlockedSession() const { return m_blocked; }
signals:
    void cookiesBlocked(int total);
private:
    QWebEngineProfile *m_profile;
    int m_policy = 0;   // captured on applyPolicy, used on IO thread
    std::atomic<int> m_blocked{0};
};
