#include "WebPage.h"
#include "AppSettings.h"
#include "Database.h"
#include "ThemeManager.h"

#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QCheckBox>
#include <QClipboard>
#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QGuiApplication>
#include <QVBoxLayout>

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    connect(this, &QWebEnginePage::featurePermissionRequested, this,
            &WebPage::onFeaturePermissionRequested);
    connect(this, &QWebEnginePage::fullScreenRequested, this, &WebPage::onFullScreenRequested);
    connect(this, &QWebEnginePage::certificateError, this, &WebPage::onCertificateError);
}

static QWidget *pageHostWidget(QWebEnginePage *p)
{
    // walk QObject parents to the nearest widget window
    for (QObject *o = p->parent(); o; o = o->parent())
        if (auto *w = qobject_cast<QWidget *>(o))
            return w->window();
    return nullptr;
}

QString WebPage::featureName(QWebEnginePage::Feature f)
{
    switch (f) {
    case QWebEnginePage::Geolocation: return "Location";
    case QWebEnginePage::MediaAudioCapture: return "Microphone";
    case QWebEnginePage::MediaVideoCapture: return "Camera";
    case QWebEnginePage::MediaAudioVideoCapture: return "Camera and microphone";
    case QWebEnginePage::DesktopVideoCapture: return "Screen capture";
    case QWebEnginePage::DesktopAudioVideoCapture: return "Screen + audio capture";
    case QWebEnginePage::Notifications: return "Notifications";
    case QWebEnginePage::MouseLock: return "Mouse lock";
    default: return "Permission";
    }
}

QString WebPage::featureIcon(QWebEnginePage::Feature f)
{
    switch (f) {
    case QWebEnginePage::Geolocation: return "location";
    case QWebEnginePage::MediaAudioCapture: return "mic";
    case QWebEnginePage::MediaVideoCapture:
    case QWebEnginePage::MediaAudioVideoCapture:
    case QWebEnginePage::DesktopVideoCapture:
    case QWebEnginePage::DesktopAudioVideoCapture: return "camera";
    case QWebEnginePage::Notifications: return "notification";
    case QWebEnginePage::MouseLock: return "mouse";
    default: return "info";
    }
}

void WebPage::onFeaturePermissionRequested(const QUrl &origin, QWebEnginePage::Feature feature)
{
    const QString fkey = featureName(feature);
    const QString originKey = origin.toString();

    // stored decision?
    const int stored = Database::instance()->permission(originKey, fkey, -1);
    if (stored == 1) { setFeaturePermission(origin, feature, QWebEnginePage::PermissionGrantedByUser); return; }
    if (stored == 2) { setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser); return; }

    // default policy from settings (0 ask, 1 allow, 2 block)
    const int def = AppSettings::instance()->permissionDefault(fkey.toLower());
    if (def == 1) { setFeaturePermission(origin, feature, QWebEnginePage::PermissionGrantedByUser); return; }
    if (def == 2) { setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser); return; }

    // ask the user (modal, deferred decision)
    PermissionDialog dlg(pageHostWidget(this), origin, feature);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.rememberChoice())
            Database::instance()->setPermission(originKey, fkey,
                                                dlg.choice() == QWebEnginePage::PermissionGrantedByUser ? 1 : 2);
        setFeaturePermission(origin, feature, dlg.choice());
    } else {
        if (dlg.rememberChoice())
            Database::instance()->setPermission(originKey, fkey, 2);
        setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser);
    }
}

void WebPage::onFullScreenRequested(QWebEngineFullScreenRequest request)
{
    request.accept();
    QWidget *w = pageHostWidget(this);
    if (!w) return;
    if (request.toggleOn())
        w->showFullScreen();
    else
        w->showNormal();
}

void WebPage::onCertificateError(const QWebEngineCertificateError &error)
{
    QWebEngineCertificateError err = error; // copy to be able to respond
    m_hadCertError = true;
    emit pageSecurityChanged();
    if (m_certOverrides.contains(err.url().host())) {
        err.acceptCertificate();
        return;
    }
    if (!err.isOverridable()) {
        QMessageBox::warning(pageHostWidget(this), "Security warning",
                             QStringLiteral("Connection blocked: the certificate for %1 is invalid "
                                             "and cannot be bypassed.\n\n%2")
                                 .arg(err.url().host(), err.description()));
        err.rejectCertificate();
        return;
    }
    CertificateDialog dlg(pageHostWidget(this), err);
    if (dlg.exec() == QDialog::Accepted && dlg.proceed()) {
        m_certOverrides.insert(err.url().host());
        err.acceptCertificate();
    } else {
        err.rejectCertificate();
    }
}

bool WebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (isMainFrame && type == QWebEnginePage::NavigationTypeTyped)
        resetCertError();
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage *WebPage::createWindow(WebWindowType type)
{
    if (m_createWindowHandler)
        return m_createWindowHandler(type);
    return nullptr;
}

void WebPage::saveAsPdf(const QString &filePath)
{
    QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
    printToPdf(filePath, layout);
}

void WebPage::saveHtml(const QString &filePath)
{
    toHtml([filePath](const QString &html) {
        if (html.isEmpty()) return;
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(html.toUtf8());
    });
}

void WebPage::selectAllAndCopy()
{
    triggerAction(QWebEnginePage::SelectAll);
    triggerAction(QWebEnginePage::Copy);
    Q_UNUSED(this);
}

void WebPage::findNext(const QString &text, bool forward, bool caseSensitive)
{
    QWebEnginePage::FindFlags flags;
    if (!forward) flags |= QWebEnginePage::FindBackward;
    if (caseSensitive) flags |= QWebEnginePage::FindCaseSensitively;
    findText(text, flags);
}

// ---------------------------------------------------------------- dialogs

PermissionDialog::PermissionDialog(QWidget *parent, const QUrl &origin, QWebEnginePage::Feature feature)
    : QDialog(parent)
{
    setModal(true);
    setWindowTitle(QStringLiteral("Permission requested"));
    setMinimumWidth(420);
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout();
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(Icons::pixmap(WebPage::featureIcon(feature),
                                       ThemeManager::instance()->color(RoleText), 32));
    top->addWidget(iconLabel);
    auto *txt = new QLabel(QStringLiteral("<b>%1</b> wants to use:<br><b style=\"font-size:15px\">%2</b>")
                               .arg(origin.host().toHtmlEscaped(), WebPage::featureName(feature)), this);
    txt->setTextFormat(Qt::RichText);
    top->addWidget(txt, 1);
    lay->addLayout(top);
    m_remember = new QCheckBox(QStringLiteral("Always allow this for ") + origin.host(), this);
    lay->addWidget(m_remember);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No, this);
    btns->button(QDialogButtonBox::Yes)->setText("Allow");
    btns->button(QDialogButtonBox::No)->setText("Block");
    connect(btns, &QDialogButtonBox::accepted, this, [this] {
        m_choice = QWebEnginePage::PermissionGrantedByUser;
        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, [this] {
        m_choice = QWebEnginePage::PermissionDeniedByUser;
        accept();
    });
    lay->addWidget(btns);
}

bool PermissionDialog::rememberChoice() const { return m_remember->isChecked(); }

CertificateDialog::CertificateDialog(QWidget *parent, const QWebEngineCertificateError &err)
    : QDialog(parent)
{
    setModal(true);
    setWindowTitle(QStringLiteral("Your connection is not private"));
    setMinimumWidth(460);
    auto *lay = new QVBoxLayout(this);
    auto *icon = new QLabel(this);
    icon->setPixmap(Icons::pixmap("warn", QColor("#d93025"), 40));
    lay->addWidget(icon, 0, Qt::AlignHCenter);
    auto *title = new QLabel(QStringLiteral("<h3 style=\"color:#d93025\">Your connection is not private</h3>"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setTextFormat(Qt::RichText);
    lay->addWidget(title);
    auto *body = new QLabel(QStringLiteral(
        "Attackers may be trying to steal your information from <b>%1</b>.<br><br>"
        "<small>%2</small><br><br>"
        "You can proceed at your own risk, or go back to safety.")
        .arg(err.url().host().toHtmlEscaped(), err.description().toHtmlEscaped()), this);
    body->setWordWrap(true);
    lay->addWidget(body);
    auto *btns = new QDialogButtonBox(this);
    QPushButton *back = btns->addButton(QStringLiteral("Back to safety"), QDialogButtonBox::RejectRole);
    QPushButton *adv = btns->addButton(QStringLiteral("Proceed to site (unsafe)"), QDialogButtonBox::AcceptRole);
    connect(back, &QPushButton::clicked, this, [this] { m_proceed = false; reject(); });
    connect(adv, &QPushButton::clicked, this, [this] { m_proceed = true; accept(); });
    lay->addWidget(btns);
}
