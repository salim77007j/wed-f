#include "WebView.h"
#include "AppSettings.h"
#include "Database.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QWebEngineContextMenuRequest>

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    connect(this, &QWebEngineView::renderProcessTerminated, this, [] {});
    // Ctrl+wheel zoom lives on the focus proxy (Chromium's internal widget)
    if (QWidget *fp = focusProxy())
        fp->installEventFilter(this);
}

void WebView::showEvent(QShowEvent *event)
{
    QWebEngineView::showEvent(event);
    if (QWidget *fp = focusProxy())
        fp->installEventFilter(this);
}

void WebView::setZoomFromSiteMemory(const QUrl &u)
{
    const QString host = Utils::hostOfUrl(u);
    if (host.isEmpty()) return;
    const int stored = Database::instance()->zoom(host, AppSettings::instance()->defaultZoom());
    setZoomFactor(stored / 100.0);
    emit zoomChanged(stored);
}

void WebView::applyZoomDelta(int deltaSteps)
{
    const int current = qRound(zoomFactor() * 100);
    const int next = qBound(25, current + deltaSteps * 10, 500);
    if (next == current) return;
    setZoomFactor(next / 100.0);
    const QString host = Utils::hostOfUrl(url());
    if (!host.isEmpty())
        Database::instance()->setZoom(host, next);
    emit zoomChanged(next);
}

void WebView::resetZoom()
{
    setZoomFactor(1.0);
    const QString host = Utils::hostOfUrl(url());
    if (!host.isEmpty())
        Database::instance()->setZoom(host, 100);
    emit zoomChanged(100);
}

bool WebView::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(ev);
        if (we->modifiers() & Qt::ControlModifier) {
            applyZoomDelta(we->angleDelta().y() > 0 ? 1 : -1);
            return true;
        }
    }
    return QWebEngineView::eventFilter(obj, ev);
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    if (m_handler)
        return m_handler(type);
    return nullptr;
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    const QWebEngineContextMenuRequest *req = lastContextMenuRequest();
    QMenu *menu = createStandardContextMenu();
    if (!menu)
        return;

    // drop Chromium's built-in "Inspect element" (we provide the wired one)
    const QList<QAction *> before = menu->actions();
    for (QAction *a : before) {
        const QString t = a->text().remove('&');
        if (t.compare("inspect element", Qt::CaseInsensitive) == 0
            || t.compare("inspect", Qt::CaseInsensitive) == 0) {
            menu->removeAction(a);
            a->deleteLater();
        }
    }

    menu->addSeparator();

    if (req && !req->linkUrl().isEmpty() && (req->linkUrl().scheme() == "http" || req->linkUrl().scheme() == "https")) {
        QAction *openBg = new QAction(tr("Open link in new background tab"), menu);
        connect(openBg, &QAction::triggered, this, [this, url = req->linkUrl()] {
            emit openLinkRequested(url, OpenBackground);
        });
        menu->addAction(openBg);

        QAction *openFg = new QAction(tr("Open link in new tab"), menu);
        connect(openFg, &QAction::triggered, this, [this, url = req->linkUrl()] {
            emit openLinkRequested(url, {});
        });
        menu->addAction(openFg);

        QAction *openPrivate = new QAction(Icons::themed("private"), tr("Open link in private window"), menu);
        connect(openPrivate, &QAction::triggered, this, [this, url = req->linkUrl()] {
            emit openLinkRequested(url, OpenPrivate);
        });
        menu->addAction(openPrivate);

        QAction *bookmarkLink = new QAction(Icons::themed("star"), tr("Bookmark link"), menu);
        connect(bookmarkLink, &QAction::triggered, this, [this, url = req->linkUrl()] {
            emit bookmarkLinkRequested(url, url.toString());
        });
        menu->addAction(bookmarkLink);
    }

    if (req && !req->selectedText().isEmpty()) {
        QString engineName = QStringLiteral("default search engine");
        for (const auto &e : AppSettings::searchEngines()) {
            if (e.id == AppSettings::instance()->searchEngine()) engineName = e.name;
        }
        QString text = req->selectedText();
        if (text.length() > 24) text = text.left(21) + "...";
        QAction *searchSel = new QAction(Icons::themed("search"),
                                         tr("Search %1 for \"%2\"").arg(engineName, text), menu);
        connect(searchSel, &QAction::triggered, this, [this, t = req->selectedText()] {
            emit searchSelectionRequested(t);
        });
        menu->addAction(searchSel);
    }

    QAction *inspect = new QAction(Icons::themed("devtools"), tr("Inspect element"), menu);
    connect(inspect, &QAction::triggered, this, [this] { emit devToolsRequested(); });
    menu->addAction(inspect);

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}
