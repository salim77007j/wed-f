#include "ProfileCatalog.h"
#include "AppSettings.h"
#include "PrivacyEngine.h"
#include "RequestInterceptor.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

ProfileCatalog *ProfileCatalog::instance()
{
    static ProfileCatalog *s_c = new ProfileCatalog();
    return s_c;
}

ProfileCatalog::ProfileCatalog(QObject *parent)
    : QObject(parent)
{
    const QString root = storageRoot();
    QDir().mkpath(root);

    m_persistent = new QWebEngineProfile("Persistent", this);
    m_persistent->setPersistentStoragePath(root + "/storage");
    m_persistent->setCachePath(root + "/cache");
    m_persistent->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_persistent->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    m_persistent->setHttpAcceptLanguage("en-US,en;q=0.9");
    m_incognito = new QWebEngineProfile(this); // off-the-record
    m_incognito->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    m_incognito->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    applyUserAgent();
    applyFingerprintScript();
    attachAll();

    connect(AppSettings::instance(), &AppSettings::privacyChanged, this,
            &ProfileCatalog::reloadPrivacyPolicies);
    connect(AppSettings::instance(), &AppSettings::userAgentChanged, this,
            &ProfileCatalog::applyUserAgent);
    connect(AppSettings::instance(), &AppSettings::filtersChanged, this, []() {
        PrivacyEngine::instance()->reload();
    });
}

QString ProfileCatalog::storageRoot()
{
    QString root = qEnvironmentVariable("WED_PROFILE");
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wed";
    return root;
}

void ProfileCatalog::attachAll()
{
    if (m_attached) return;
    m_attached = true;

    m_interceptor = new RequestInterceptor(this);
    m_persistent->setUrlRequestInterceptor(m_interceptor);
    m_incognito->setUrlRequestInterceptor(m_interceptor);

    m_cookieJarPersistent = new CookieJar(m_persistent, this);
    m_cookieJarIncognito = new CookieJar(m_incognito, this);
    connect(AppSettings::instance(), &AppSettings::privacyChanged, this, [this] {
        if (m_cookieJarPersistent) m_cookieJarPersistent->applyPolicy();
        if (m_cookieJarIncognito) m_cookieJarIncognito->applyPolicy();
    });
}

void ProfileCatalog::applyUserAgent()
{
    AppSettings *s = AppSettings::instance();
    QString ua;
    switch (s->userAgentMode()) {
    case 1: // Chrome-compatible
        ua = QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                            "Chrome/131.0.0.0 Safari/537.36");
        break;
    case 2: // Firefox-compatible
        ua = QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:132.0) Gecko/20100101 Firefox/132.0");
        break;
    case 3: // custom
        if (!s->customUserAgent().isEmpty()) ua = s->customUserAgent();
        break;
    default: break; // 0 = WED default (Chromium default with brand token)
    }
    // default: append WED brand to the default QtWebEngine UA
    if (ua.isEmpty()) {
        QString base = m_persistent->httpUserAgent();
        if (!base.contains("WED/"))
            ua = base + " WED/1.0";
    }
    m_persistent->setHttpUserAgent(ua);
    m_incognito->setHttpUserAgent(ua);
}

void ProfileCatalog::applyFingerprintScript()
{
    QFile f(":/js/fingerprint.js");
    if (!f.open(QIODevice::ReadOnly)) return;
    const QString src = QString::fromUtf8(f.readAll());
    f.close();

    const bool enabled = AppSettings::instance()->fingerprintProtection();
    for (QWebEngineProfile *p : {m_persistent, m_incognito}) {
        QWebEngineScriptCollection *coll = p->scripts();
        // remove ours
        const QList<QWebEngineScript> existing = coll->toList();
        for (const QWebEngineScript &s : existing) {
            if (s.name() == "wed-fingerprint")
                coll->remove(s);
        }
        if (!enabled) continue;
        QWebEngineScript script;
        script.setName("wed-fingerprint");
        script.setSourceCode(src);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(true);
        script.setWorldId(QWebEngineScript::MainWorld);
        coll->insert(script);
    }
}

void ProfileCatalog::reloadPrivacyPolicies()
{
    applyFingerprintScript();
    // cookie policies
    AppSettings *s = AppSettings::instance();
    m_persistent->setPersistentCookiesPolicy(
        s->cookiePolicy() == AppSettings::CookiesBlockAll
            ? QWebEngineProfile::NoPersistentCookies
            : QWebEngineProfile::AllowPersistentCookies);
}
