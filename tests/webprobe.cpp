// Minimal WebEngine probe: load a URL, dump DOM size, screenshot.
// Isolates WED app code from raw Qt WebEngine behavior.
#include <QApplication>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEnginePage>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString url = argc > 1 ? QString::fromLatin1(argv[1]) : QStringLiteral("https://example.org");
    const int waitMs = argc > 2 ? QByteArray(argv[2]).toInt() : 8000;

    auto *v = new QWebEngineView;
    v->resize(1280, 900);
    v->show();
    v->load(QUrl(url));

    QTimer::singleShot(waitMs, [v, url]() {
        v->page()->toHtml([v, url](const QString &html) {
            std::printf("[PROBE] url=%s DOM bytes=%lld\n", qPrintable(url), (long long)html.toUtf8().size());
            std::fflush(stdout);
            v->grab().save(QStringLiteral("/tmp/probe-page.png"), "PNG");
            std::printf("[PROBE] screenshot saved\n");
            std::fflush(stdout);
            QApplication::exit(0);
        });
    });
    return app.exec();
}
