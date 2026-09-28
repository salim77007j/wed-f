// WED Browser — application entry point.
// Responsibilities: early Qt/Chromium setup, single-instance guard, service
// startup (settings, database, profiles, privacy engine, theme), crash-recovery
// prompt, window lifecycle (multi-window, private windows), URL forwarding
// from second instances.

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

#include "MainWindow.h"
#include "AppSettings.h"
#include "Database.h"
#include "ProfileCatalog.h"
#include "PrivacyEngine.h"
#include "SessionManager.h"
#include "ThemeManager.h"
#include "Utils.h"

static constexpr char kInstanceKey[] = "wed-f-single-instance";

static QList<MainWindow *> g_windows;

static void createWindow(bool privateMode, const QStringList &urls)
{
    MainWindow *w = new MainWindow(privateMode, urls);
    w->setAttribute(Qt::WA_DeleteOnClose);
    g_windows.append(w);
    QObject::connect(w, &MainWindow::newWindowRequested, &createWindow);
    QObject::connect(w, &MainWindow::lastWindowClosed, [w] {
        // window closes itself; drop from registry
        g_windows.removeAll(w);
        if (g_windows.isEmpty())
            QApplication::quit();
    });
    w->show();
    QApplication::setActiveWindow(w);
}

// bring the newest window forward and open forwarded URLs in it
static void openForwardedUrls(const QStringList &urls)
{
    for (const QString &u : urls) {
        if (u.trimmed().isEmpty())
            continue;
        MainWindow *target = g_windows.isEmpty() ? nullptr
                            : g_windows.last();
        if (!target) {
            createWindow(false, { u });
            continue;
        }
        target->tabWidget()->newTab(target->isPrivate(), false, QUrl::fromUserInput(u));
        target->raise();
        target->activateWindow();
    }
}

int main(int argc, char *argv[])
{
    // Chromium sandbox cannot run in rootless containers without user namespaces;
    // we run as a normal user normally — keep default behavior unless overridden.
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS") && ::getuid() == 0)
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--no-sandbox");
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("wed"));
    QApplication::setApplicationDisplayName(QStringLiteral("WED Browser"));
    QApplication::setOrganizationName(QStringLiteral("wed"));
    QApplication::setWindowIcon(QIcon(":/icons/app.png"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("WED — a fast, private, native web browser."));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption privateOpt(QStringLiteral("private"),
                                  QStringLiteral("Start a private window."));
    cli.addOption(privateOpt);
    cli.addPositionalArgument(QStringLiteral("urls"),
                              QStringLiteral("URLs to open in new tabs."), QStringLiteral("[urls...]"));
    cli.process(app);

    // ---------------- single instance ----------------
    {
        QLocalSocket probe;
        probe.connectToServer(QLatin1String(kInstanceKey));
        if (probe.waitForConnected(300)) {
            // send URLs, ask primary to raise, then exit
            const QStringList pos = cli.positionalArguments();
            const QString payload = (pos.isEmpty() ? QString(QStringLiteral("raise")) : pos.join(QLatin1Char('\n')));
            probe.write(payload.toUtf8());
            probe.waitForBytesWritten(300);
            probe.disconnectFromServer();
            return 0;
        }
    }
    QLocalServer *server = new QLocalServer();
    QLocalServer::removeServer(QLatin1String(kInstanceKey));
    server->listen(QLatin1String(kInstanceKey));
    QObject::connect(server, &QLocalServer::newConnection, [server] {
        QLocalSocket *sock = server->nextPendingConnection();
        if (!sock)
            return;
        QObject::connect(sock, &QLocalSocket::disconnected, sock, &QLocalSocket::deleteLater);
        QObject::connect(sock, &QLocalSocket::readyRead, [sock] {
            const QStringList lines = QString::fromUtf8(sock->readAll())
                                          .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (lines.size() == 1 && lines.first() == QLatin1String("raise")) {
                if (!g_windows.isEmpty()) {
                    g_windows.last()->showNormal();
                    g_windows.last()->raise();
                    g_windows.last()->activateWindow();
                }
            } else {
                openForwardedUrls(lines);
            }
        });
    });

    // ---------------- services ----------------
    AppSettings::instance();                       // settings INI
    Database::instance();                          // SQLite: history/bookmarks/stats
    ThemeManager::instance()->apply();             // QSS theming
    ProfileCatalog::instance()->attachAll();       // profiles + interceptor + cookie jar + scripts
    PrivacyEngine::instance()->start();            // Rust core: load filter lists

    const QStringList urlArgs = cli.positionalArguments();
    const bool privateMode = cli.isSet(privateOpt);

    // ---------------- crash recovery ----------------
    if (!privateMode && SessionManager::instance()->crashedLastRun()
        && AppSettings::instance()->startupMode() != AppSettings::StartupContinue) {
        QMessageBox mb(QMessageBox::Warning, QStringLiteral("WED"),
                       QStringLiteral("WED didn't shut down correctly.\n\n"
                                      "Restore the pages from your previous session?"),
                       QMessageBox::Yes | QMessageBox::No);
        mb.button(QMessageBox::Yes)->setText(QObject::tr("Restore"));
        mb.button(QMessageBox::No)->setText(QObject::tr("Start fresh"));
        if (mb.exec() == QMessageBox::Yes) {
            // window is created empty, then we restore into it below
            createWindow(false, QStringList());
            MainWindow *w = g_windows.last();
            const SessionManager::SessionData d = SessionManager::instance()->loadLastSession();
            if (!d.tabs.isEmpty()) {
                w->tabWidget()->restoreSession(d.tabs, d.currentIndex);
                if (w->tabWidget()->count() > 1)
                    w->tabWidget()->closeTab(0); // drop the auto-created start tab
            }
            return app.exec();
        }
    }

    // ---------------- normal startup ----------------
    createWindow(privateMode, urlArgs);

    return app.exec();
}
