#include "RequestInterceptor.h"
#include "PrivacyEngine.h"
#include "AppSettings.h"
#include "Database.h"
#include "Utils.h"

#include <QUrl>
#include <atomic>

RequestInterceptor::RequestInterceptor(QObject *parent)
    : QObject(parent), QWebEngineUrlRequestInterceptor()
{
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    AppSettings *s = AppSettings::instance();

    // privacy headers on all requests
    if (s->dntHeader())
        info.setHttpHeader("DNT", "1");
    if (s->gpcHeader()) {
        info.setHttpHeader("Sec-GPC", "1");
        info.setHttpHeader("GPC", "1");
    }

    const QUrl reqUrl = info.requestUrl();
    const QUrl firstParty = info.firstPartyUrl();

    // HTTPS-first: block insecure main-frame navigations (insecure subresources
    // are already flagged by Chromium mixed-content rules)
    if (s->httpsFirst() && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        && reqUrl.scheme() == "http" && !reqUrl.host().isEmpty()
        && reqUrl.host() != "localhost" && !reqUrl.host().endsWith(".local")) {
        info.redirect(QUrl("https://" + reqUrl.host() + reqUrl.path()
                           + (reqUrl.hasQuery() ? "?" + reqUrl.query() : QString())));
    }

    if (reqUrl.scheme() != "http" && reqUrl.scheme() != "https" && reqUrl.scheme() != "ws"
        && reqUrl.scheme() != "wss")
        return; // internal schemes (wed://, file://...) untouched

    int d = PrivacyEngine::instance()->check(reqUrl, firstParty, int(info.resourceType()));
    if (d != PrivacyEngine::Allowed)
        info.block(true);
}

// ---------------------------------------------------------------- CookieJar

CookieJar::CookieJar(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile)
{
    applyPolicy();
}

void CookieJar::applyPolicy()
{
    AppSettings *s = AppSettings::instance();
    m_policy = s->cookiePolicy();
    QWebEngineCookieStore *store = m_profile->cookieStore();

    // Veto cookies before they are stored: full third-party / all-cookie blocking.
    // The policy value is captured here (main thread); no settings access on IO threads.
    const int policy = m_policy;
    store->setCookieFilter([this, policy](const QWebEngineCookieStore::FilterRequest &req) -> bool {
        if (policy == AppSettings::CookiesAllowAll)
            return true;
        if (policy == AppSettings::CookiesBlockAll) {
            m_blocked.fetch_add(1);
            return false;
        }
        // block third-party cookies
        if (req.thirdParty) {
            m_blocked.fetch_add(1);
            return false;
        }
        return true;
    });
}
