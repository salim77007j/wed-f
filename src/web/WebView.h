#pragma once
#include <QtWebEngineWidgets/QWebEngineView>

// QWebEngineView with createWindow routing, Ctrl+wheel zoom, per-site zoom memory,
// context menu = Chromium's standard menu + real custom actions.
class WebView : public QWebEngineView
{
    Q_OBJECT
public:
    enum OpenFlag { OpenBackground = 0x1, OpenPrivate = 0x2 };
    Q_DECLARE_FLAGS(OpenFlags, OpenFlag)

    explicit WebView(QWidget *parent = nullptr);

    // shell installs this to create tabs/windows/popups for window.open()
    void setCreateViewHandler(std::function<QWebEngineView *(QWebEnginePage::WebWindowType)> h)
    { m_handler = std::move(h); }

    void applyZoomDelta(int deltaSteps);   // ±10% steps, per-site memory
    void resetZoom();
    void setZoomFromSiteMemory(const QUrl &url);

signals:
    void zoomChanged(int percent);
    void openLinkRequested(const QUrl &url, OpenFlags flags);
    void searchSelectionRequested(const QString &text);
    void devToolsRequested();
    void bookmarkLinkRequested(const QUrl &url, const QString &title);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void showEvent(QShowEvent *event) override;

private:
    std::function<QWebEngineView *(QWebEnginePage::WebWindowType)> m_handler;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(WebView::OpenFlags)
