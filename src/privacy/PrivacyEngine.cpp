#include "PrivacyEngine.h"
#include "AppSettings.h"
#include "Database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

// Content-type bits (must match src/core/src/filters.rs CT_*)
namespace {
constexpr int CT_SCRIPT = 1;
constexpr int CT_IMAGE = 2;
constexpr int CT_STYLESHEET = 4;
constexpr int CT_OBJECT = 8;
constexpr int CT_XHR = 16;
constexpr int CT_SUBDOCUMENT = 32;
constexpr int CT_PING = 64;
constexpr int CT_MEDIA = 128;
constexpr int CT_FONT = 256;
constexpr int CT_OTHER = 512;
constexpr int CT_WEBSOCKET = 1024;
constexpr int CT_MAINFRAME = 2048;

QString listsDir()
{
    QString base = qEnvironmentVariable("WED_PROFILE");
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wed";
    return base + "/lists";
}
}

PrivacyEngine *PrivacyEngine::instance()
{
    static PrivacyEngine *s_e = new PrivacyEngine();
    return s_e;
}

PrivacyEngine::PrivacyEngine(QObject *parent)
    : QObject(parent)
{
    m_core = wed_core_create();
    refreshExceptions();
    connect(&m_flushTimer, &QTimer::timeout, this, &PrivacyEngine::flushStats);
}

PrivacyEngine::~PrivacyEngine()
{
    if (m_core) {
        wed_core_destroy(m_core);
        m_core = nullptr;
    }
}

void PrivacyEngine::start()
{
    loadListsImpl();
    m_flushTimer.start(2000);
}

void PrivacyEngine::loadListsImpl()
{
    QMutexLocker lock(&m_mutex);
    // recreate engine to drop old rules
    if (m_core) wed_core_destroy(m_core);
    m_core = wed_core_create();
    m_sessionAds = 0;
    m_sessionTrackers = 0;

    const QString dir = listsDir();
    QDir().mkpath(dir);
    // Extract bundled lists from resources on first run (allows later updates)
    struct Res { const char *res; const char *file; };
    const Res bundled[] = {{":/lists/easylist.txt", "easylist.txt"},
                           {":/lists/easyprivacy.txt", "easyprivacy.txt"}};
    for (const Res &r : bundled) {
        const QString target = dir + "/" + r.file;
        if (!QFile::exists(target))
            QFile::copy(QLatin1String(r.res), target);
    }

    AppSettings *s = AppSettings::instance();
    int rules = 0;
    if (s->easyListEnabled()) {
        const QString p = dir + "/easylist.txt";
        if (QFile::exists(p))
            rules += wed_core_load_list(m_core, p.toUtf8().constData(), ListAd);
    }
    if (s->easyPrivacyEnabled()) {
        const QString p = dir + "/easyprivacy.txt";
        if (QFile::exists(p))
            rules += wed_core_load_list(m_core, p.toUtf8().constData(), ListTracker);
    }
    // custom lists: any *.txt the user added beyond the two bundled names
    const auto entries = QDir(dir).entryList({"*.txt"}, QDir::Files);
    for (const QString &name : entries) {
        if (name == "easylist.txt" || name == "easyprivacy.txt") continue;
        rules += wed_core_load_list(m_core, (dir + "/" + name).toUtf8().constData(), ListCustom);
    }
    syncAllowlistLocked();
    emit listsLoaded(rules);
}

void PrivacyEngine::reload() { loadListsImpl(); }

void PrivacyEngine::syncAllowlistLocked()
{
    const QString joined = m_exceptions.join('\n');
    wed_core_set_allowlist(m_core, joined.toUtf8().constData());
}


void PrivacyEngine::refreshExceptions()
{
    QMutexLocker ex(&m_exMutex);
    m_exceptions = Database::instance()->shieldExceptions();
}

bool PrivacyEngine::shieldsEnabledFor(const QString &host) const
{
    QMutexLocker ex(&m_exMutex);
    const QString h = host.toLower();
    for (const QString &e : m_exceptions) {
        if (h == e || h.endsWith("." + e)) return false;
    }
    return true;
}

void PrivacyEngine::setShieldException(const QString &host, bool off)
{
    Database::instance()->setShieldException(host, off);
    refreshExceptions();
}

int PrivacyEngine::check(const QUrl &requestUrl, const QUrl &firstPartyUrl, int resourceType)
{
    AppSettings *s = AppSettings::instance();
    if (!s->blockAds() && !s->blockTrackers()) return Allowed;

    const QString pageHost = Utils::hostOfUrl(firstPartyUrl);
    if (!shieldsEnabledFor(pageHost)) return Allowed;

    QMutexLocker lock(&m_mutex);
    int d = wed_core_check(m_core,
                           requestUrl.toString().toUtf8().constData(),
                           firstPartyUrl.toString().toUtf8().constData(),
                           resourceTypeToBitmask(resourceType));
    if (d == BlockedAd && !s->blockAds()) d = Allowed;
    if ((d == BlockedTracker || d == BlockedCustom) && !s->blockTrackers()) d = Allowed;
    if (d != Allowed) {
        unsigned long long a = 0, t = 0;
        wed_core_stats(m_core, &a, &t);
        m_sessionAds = a;
        m_sessionTrackers = t;
        quint32 *slot = m_pending[pageHost];
        if (slot) {
            if (d == BlockedAd) slot[0]++;
            else slot[1]++;
        }
        const QString reqHost = Utils::hostOfUrl(requestUrl);
        // queued emission to main thread receivers
        emit blocked(firstPartyUrl.toString(), reqHost, d);
    }
    return d;
}

void PrivacyEngine::flushStats()
{
    QMutexLocker lock(&m_mutex);
    if (m_pending.isEmpty()) return;
    const auto keys = m_pending.keys();
    for (const QString &host : keys) {
        const quint32 *v = m_pending.value(host);
        if (v[0] || v[1] || v[2])
            Database::instance()->incrementStats(host, int(v[0]), int(v[1]), int(v[2]));
        m_pending.remove(host);
    }
}


QString PrivacyEngine::cosmeticFor(const QString &host)
{
    if (host.isEmpty()) return {};
    QMutexLocker lock(&m_mutex);
    char *json = wed_core_cosmetic(m_core, host.toUtf8().constData());
    if (!json) return {};
    QString out = QString::fromUtf8(json);
    wed_core_free_string(json);
    return out;
}

long long PrivacyEngine::ruleCount() const
{
    return wed_core_rule_count(m_core);
}

unsigned long long PrivacyEngine::sessionAds() const { return m_sessionAds; }
unsigned long long PrivacyEngine::sessionTrackers() const { return m_sessionTrackers; }

int PrivacyEngine::resourceTypeToBitmask(int qtResourceType)
{
    switch (qtResourceType) {
    case 0: return CT_MAINFRAME;
    case 1: return CT_SUBDOCUMENT;
    case 2: return CT_STYLESHEET;
    case 3: return CT_SCRIPT;
    case 4: return CT_IMAGE;
    case 5: return CT_FONT;
    case 6: return CT_OTHER;
    case 7: return CT_OBJECT;
    case 8: return CT_MEDIA;
    case 9: case 10: case 15: return CT_SCRIPT;
    case 11: return CT_OTHER;
    case 12: return CT_IMAGE;
    case 13: return CT_XHR;
    case 14: return CT_PING;
    case 16: return CT_OTHER;
    case 17: return CT_OBJECT;
    default: return CT_OTHER;
    }
}
