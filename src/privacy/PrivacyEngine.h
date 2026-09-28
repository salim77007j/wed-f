#pragma once
#include <QObject>
#include <QString>
#include <QMutex>
#include <QHash>
#include <QTimer>

// C ABI from the Rust core (see src/core/src/lib.rs)
extern "C" {
typedef void *WedCore;
WedCore wed_core_create();
void wed_core_destroy(WedCore core);
int wed_core_load_list(WedCore core, const char *path, int category);
void wed_core_set_allowlist(WedCore core, const char *hosts);
void wed_core_allow_site(WedCore core, const char *host, int allowed);
int wed_core_is_allowlisted(WedCore core, const char *host);
int wed_core_check(WedCore core, const char *url, const char *first_party, int resource_type);
char *wed_core_cosmetic(WedCore core, const char *host);
void wed_core_cosmetic_count(WedCore core, const char *host, long long *generic_out, long long *specific_out);
void wed_core_free_string(char *s);
void wed_core_stats(WedCore core, unsigned long long *ads, unsigned long long *trackers);
long long wed_core_rule_count(WedCore core);
}

// Central privacy orchestrator. `check()` is called from Chromium IO threads;
// everything DB-related is deferred to the main thread (flush timer).
class PrivacyEngine : public QObject
{
    Q_OBJECT
public:
    enum Decision { Allowed = 0, BlockedAd = 1, BlockedTracker = 2, BlockedCustom = 3 };
    enum ListCategory { ListAd = 0, ListTracker = 1, ListCustom = 2 };

    static PrivacyEngine *instance();

    // thread-safe: called from Chromium IO thread
    int check(const QUrl &requestUrl, const QUrl &firstPartyUrl, int resourceType);
    // thread-safe: cosmetic stylesheet JSON for host
    QString cosmeticFor(const QString &host);

    // main thread API
    void start();          // load lists + start flush timer
    void reload();         // reload lists honoring settings
    bool shieldsEnabledFor(const QString &host) const;
    void setShieldException(const QString &host, bool off);
    void refreshExceptions();
    long long ruleCount() const;
    unsigned long long sessionAds() const;
    unsigned long long sessionTrackers() const;
    static int resourceTypeToBitmask(int qtResourceType);

signals:
    void blocked(const QString &firstPartyUrl, const QString &requestHost, int category);
    void listsLoaded(int rules);

private slots:
    void flushStats();

private:
    explicit PrivacyEngine(QObject *parent = nullptr);
    ~PrivacyEngine() override;
    void loadListsImpl();

    WedCore m_core = nullptr;
    QMutex m_mutex;                 // guards m_core calls + pending stats
    QMutex m_exMutex;
    QStringList m_exceptions;
    QHash<QString, quint32[3]> m_pending; // host -> {ads, trackers, cookies}
    QTimer m_flushTimer;
    unsigned long long m_sessionAds = 0;
    unsigned long long m_sessionTrackers = 0;
};
