// WED Browser self-test harness.
// Drives the real application window (real widgets, real WebEngine) under
// xvfb, exercises core features, captures screenshots, and reports PASS/FAIL.
// Exit code 0 = all critical checks passed.
//
// Usage: wed-selftest [outputDir]
// Env:   WED_PROFILE (isolated profile dir), WED_SHOTS (same as arg)

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTimer>
#include <QWidget>
#include <QLibraryInfo>
#include <QTableWidget>
#include <QTabWidget>

#include "MainWindow.h"
#include "AddressBar.h"
#include "AppSettings.h"
#include "BrowserTab.h"
#include "Database.h"
#include "DownloadManager.h"
#include "FindBar.h"
#include "Panels.h"
#include "PrivacyDashboard.h"
#include "PrivacyEngine.h"
#include "ProfileCatalog.h"
#include "SessionManager.h"
#include "SettingsDialog.h"
#include "TabWidget.h"
#include "ThemeManager.h"
#include "Utils.h"
#include "WebView.h"

#include <cstdio>

static int g_failed = 0;
static int g_passed = 0;
static QString g_shotsDir;

#define CHECK(cond, name)                                                    \
    do {                                                                      \
        if (cond) { ++g_passed; std::printf("[PASS] %s\n", name); std::fflush(stdout); } \
        else      { ++g_failed; std::printf("[FAIL] %s\n", name); std::fflush(stdout); } \
    } while (0)

static void shot(QWidget *w, const QString &name)
{
    if (!w) return;
    const QString path = g_shotsDir + '/' + name + ".png";
    const QPixmap px = w->grab();
    px.save(path, "PNG");
    std::printf("[SHOT] %s (%dx%d)\n", qPrintable(path), px.width(), px.height());
    std::fflush(stdout);
}

static void step(int ms, const std::function<void()> &fn)
{
    QTimer::singleShot(ms, fn);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("wed"));
    QApplication::setOrganizationName(QStringLiteral("wed"));

    QCommandLineParser cli;
    cli.addPositionalArgument(QStringLiteral("outdir"), QStringLiteral("Screenshot output dir"));
    cli.process(app);
    g_shotsDir = cli.positionalArguments().value(0);
    if (g_shotsDir.isEmpty())
        g_shotsDir = qEnvironmentVariable("WED_SHOTS");
    if (g_shotsDir.isEmpty())
        g_shotsDir = QStringLiteral("shots");
    QDir().mkpath(g_shotsDir);

    // ---- services (same boot sequence as main.cpp) ----
    AppSettings::instance();
    Database::instance();
    ThemeManager::instance()->apply();
    ProfileCatalog::instance()->attachAll();
    PrivacyEngine::instance()->start();

    MainWindow *w = new MainWindow(false, QStringList());
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->resize(1280, 860);
    w->show();

    QElapsedTimer elapsed;
    elapsed.start();

    const QString pagesDir = QCoreApplication::applicationDirPath() + "/../tests/pages";
    const QUrl testPage = QUrl::fromLocalFile(QDir(pagesDir).absoluteFilePath("index.html"));

    // ---------------- step 0: start page ----------------
    step(1200, [&] {
        shot(w, QStringLiteral("01-startpage"));
        CHECK(w->currentTab() != nullptr, "startup created a tab");
        CHECK(w->currentTab()->isStartPage(), "new tab shows start page");
        CHECK(PrivacyEngine::instance()->ruleCount() > 1000, "filter lists loaded into Rust core");

        // privacy engine: blocked ad/tracker host decision
        const int dAd = PrivacyEngine::instance()->check(
            QUrl("https://ad.doubleclick.net/ddm/adj/x"),
            QUrl("https://example.com/"), 1);
        const int dOk = PrivacyEngine::instance()->check(
            QUrl("https://example.org/script.js"),
            QUrl("https://example.com/"), 1);
        CHECK(dAd != PrivacyEngine::Allowed, "Rust core blocks known ad host");
        CHECK(dOk == PrivacyEngine::Allowed, "Rust core allows normal host");

        // database roundtrips
        Database *db = Database::instance();
        db->addBookmark(0, QStringLiteral("Selftest"), QStringLiteral("https://example.com/"));
        CHECK(db->isBookmarked(QStringLiteral("https://example.com/")), "bookmark round-trip");
        db->addHistory(QStringLiteral("https://example.com/"), QStringLiteral("Example"));
        CHECK(db->historyCount() > 0, "history write");
        db->setPermission(QStringLiteral("https://example.com"), QStringLiteral("Geolocation"), 2);
        CHECK(db->permission(QStringLiteral("https://example.com"), QStringLiteral("Geolocation")) == 2,
              "permission persistence");
        db->setShieldException(QStringLiteral("tracker.example"), true);
        CHECK(!PrivacyEngine::instance()->shieldsEnabledFor(QStringLiteral("tracker.example")),
              "shield exception honored");

        w->navigateCurrent(testPage);
        step(2500, [&] {
            // ---------------- step 1: web page ----------------
            shot(w, QStringLiteral("02-webpage"));
            CHECK(!w->currentTab()->isStartPage(), "navigation left start page");
            CHECK(w->currentTab()->title().contains(QStringLiteral("Selftest")), "page title parsed");

            // ---------------- step 2: tabs ----------------
            auto *tw = w->tabWidget();
            BrowserTab *t2 = tw->newTab(false, true);
            CHECK(tw->count() == 2, "second tab opened");
            tw->setTabPinned(1, true);
            CHECK(tw->tabAt(1)->isPinned(), "tab pinned");
            tw->toggleMuteTab(1);
            tw->selectTab(0);
            shot(w, QStringLiteral("03-two-tabs"));
            tw->closeTab(1);
            CHECK(tw->count() == 1, "tab closed");

            // ---------------- step 3: zoom + find ----------------
            w->zoomDelta(2);
            CHECK(w->currentTab()->view()->zoomFactor() > 1.0, "zoom in applied");
            w->zoomReset();
            CHECK(qFuzzyCompare(w->currentTab()->view()->zoomFactor(), 1.0), "zoom reset");
            w->toggleFindBar();
            auto *fb = w->findChild<FindBar *>();
            CHECK(fb != nullptr && fb->isVisible(), "find bar shown");
            shot(w, QStringLiteral("04-findbar"));
            if (fb) {
                fb->hide();
            }

            // ---------------- step 4: dialogs ----------------
            step(400, [&] {
                SettingsDialog sd(w);
                sd.resize(700, 640);
                shot(&sd, QStringLiteral("05-settings"));
                CHECK(sd.findChildren<QTabWidget *>().size() >= 1, "settings dialog built");

                PrivacyDashboard pd(w);
                shot(&pd, QStringLiteral("06-privacy-dashboard"));
                CHECK(pd.findChildren<QTableWidget *>().size() >= 1, "privacy dashboard built");

                // panels
                w->showDownloadsPanel();
                shot(w, QStringLiteral("07-downloads-panel"));
                w->showHistoryPanel();
                shot(w, QStringLiteral("08-history-panel"));
                w->showBookmarksPanel();
                shot(w, QStringLiteral("09-bookmarks-panel"));

                // ---------------- step 5: session ----------------
                auto sess = w->tabWidget()->serializeTabs();
                SessionManager::instance()->save(sess, 0);
                const SessionManager::SessionData loaded = SessionManager::instance()->loadLastSession();
                CHECK(loaded.tabs.size() == sess.size(), "session save/load round-trip");
                CHECK(SessionManager::instance()->hasLastSession(), "session file exists");

                // address bar conversion
                const QUrl search = QUrl(AddressBar::searchUrlFor(QStringLiteral("hello world")));
                CHECK(search.toString().contains(QStringLiteral("duckduckgo.com")), "query → search URL");
                const QUrl url = AddressBar::inputToUrl(QStringLiteral("example.org"));
                CHECK(url.host() == QStringLiteral("example.org"), "host → URL");

                std::printf("\n=== SELFTEST COMPLETE: %d passed, %d failed ===\n", g_passed, g_failed);
                std::printf("TOTAL_ELAPSED_MS=%lld\n", elapsed.elapsed());
                std::fflush(stdout);
                // clean teardown: close the window (WA_DeleteOnClose) so pages
                // are deleted before profiles, then exit the loop
                w->close();
                QTimer::singleShot(300, [] { QApplication::exit(g_failed == 0 ? 0 : 1); });
            });
        });
    });

    return app.exec();
}
