#pragma once
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineFullScreenRequest>
#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QDialog>
#include <QCheckBox>
#include <QSet>
#include <functional>


// QWebEnginePage with real permission brokering, certificate decisions,
// session-remembered certificate overrides, and shell integration points.
class WebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit WebPage(QWebEngineProfile *profile, QObject *parent = nullptr);

    static QString featureName(QWebEnginePage::Feature f);
    static QString featureIcon(QWebEnginePage::Feature f);

    bool isCertificateOverridden(const QUrl &url) const { return m_certOverrides.contains(url.host()); }
    bool hadCertError() const { return m_hadCertError; }
    void resetCertError() { m_hadCertError = false; }

    // installed by the tab shell; must return a live page wired to a view
    void setCreateWindowHandler(std::function<QWebEnginePage *(WebWindowType)> h)
    { m_createWindowHandler = std::move(h); }

    void saveAsPdf(const QString &filePath);   // real print-to-PDF
    void saveHtml(const QString &filePath);    // serialize current DOM
    void selectAllAndCopy();
    void findNext(const QString &text, bool forward, bool caseSensitive);

signals:
    void pageSecurityChanged();

protected:
    QWebEnginePage *createWindow(WebWindowType type) override;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private slots:
    void onFeaturePermissionRequested(const QUrl &origin, QWebEnginePage::Feature feature);
    void onFullScreenRequested(QWebEngineFullScreenRequest request);
    void onCertificateError(const QWebEngineCertificateError &error);

private:
    QSet<QString> m_certOverrides;
    bool m_hadCertError = false;
    std::function<QWebEnginePage *(WebWindowType)> m_createWindowHandler;
};

// permission prompt — modal; result applied via setFeaturePermission
class PermissionDialog : public QDialog
{
    Q_OBJECT
public:
    PermissionDialog(QWidget *parent, const QUrl &origin, QWebEnginePage::Feature feature);
    bool rememberChoice() const;
    QWebEnginePage::PermissionPolicy choice() const { return m_choice; }
private:
    QWebEnginePage::PermissionPolicy m_choice = QWebEnginePage::PermissionDeniedByUser;
    QCheckBox *m_remember = nullptr;
};

// certificate warning (Chrome-style interstitial as modal dialog)
class CertificateDialog : public QDialog
{
    Q_OBJECT
public:
    CertificateDialog(QWidget *parent, const QWebEngineCertificateError &err);
    bool proceed() const { return m_proceed; }
private:
    bool m_proceed = false;
};
