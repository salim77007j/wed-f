#include "SettingsDialog.h"

#include "AppSettings.h"
#include "Database.h"
#include "PrivacyEngine.h"
#include "ProfileCatalog.h"
#include "SessionManager.h"
#include "ThemeManager.h"
#include "Utils.h"

#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QComboBox>

// =====================================================================
// ClearBrowsingDataDialog
// =====================================================================
ClearBrowsingDataDialog::ClearBrowsingDataDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Clear browsing data"));
    setModal(true);
    auto *l = new QVBoxLayout(this);
    l->addWidget(new QLabel(tr("Choose what WED should delete. This cannot be undone."), this));

    m_history = new QCheckBox(tr("Browsing history"), this);
    m_history->setChecked(true);
    m_cookies = new QCheckBox(tr("Cookies and other site data"), this);
    m_cache = new QCheckBox(tr("Cached images and files"), this);
    m_cache->setChecked(true);
    m_stats = new QCheckBox(tr("Privacy statistics (blocked ads/trackers counts)"), this);
    m_closedTabs = new QCheckBox(tr("Recently closed tabs list"), this);
    l->addWidget(m_history);
    l->addWidget(m_cookies);
    l->addWidget(m_cache);
    l->addWidget(m_stats);
    l->addWidget(m_closedTabs);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btns->button(QDialogButtonBox::Ok)->setText(tr("Clear data"));
    connect(btns, &QDialogButtonBox::accepted, this, &ClearBrowsingDataDialog::onClear);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    l->addWidget(btns);
}

void ClearBrowsingDataDialog::onClear()
{
    Database *db = Database::instance();
    if (m_history->isChecked())
        db->clearHistory();
    if (m_cookies->isChecked())
        ProfileCatalog::instance()->persistent()->cookieStore()->deleteAllCookies();
    if (m_cache->isChecked())
        ProfileCatalog::instance()->persistent()->clearHttpCache();
    if (m_stats->isChecked())
        db->clearStats();
    if (m_closedTabs->isChecked())
        db->clearClosedTabs();
    accept();
}

// =====================================================================
// SettingsDialog
// =====================================================================
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    resize(640, 620);

    auto *l = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildGeneralPage(), tr("General"));
    m_tabs->addTab(buildAppearancePage(), tr("Appearance"));
    m_tabs->addTab(buildSearchPage(), tr("Search"));
    m_tabs->addTab(buildPrivacyPage(), tr("Privacy & Security"));
    m_tabs->addTab(buildPermissionsPage(), tr("Site Permissions"));
    m_tabs->addTab(buildAdvancedPage(), tr("Advanced"));
    l->addWidget(m_tabs);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btns->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { accept(); show(); });
    l->addWidget(btns);

    loadPermissionExceptions();
}

void SettingsDialog::setCurrentPage(int page)
{
    m_tabs->setCurrentIndex(page);
}

static QFormLayout *formOf(QWidget *page)
{
    return static_cast<QFormLayout *>(page->layout());
}

QWidget *SettingsDialog::buildGeneralPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_startupMode = new QComboBox(page);
    m_startupMode->addItem(tr("Open the start page"), 0);
    m_startupMode->addItem(tr("Continue where I left off"), 1);
    m_startupMode->addItem(tr("Open these pages"), 2);
    m_startupMode->setCurrentIndex(AppSettings::instance()->startupMode());
    form->addRow(tr("On startup:"), m_startupMode);

    m_startupUrls = new QLineEdit(page);
    m_startupUrls->setText(AppSettings::instance()->startupUrls().join(QStringLiteral(", ")));
    m_startupUrls->setPlaceholderText(tr("https://example.com, https://another.org"));
    form->addRow(tr("Startup pages:"), m_startupUrls);

    m_homePage = new QLineEdit(page);
    m_homePage->setText(AppSettings::instance()->homePage());
    form->addRow(tr("Home page:"), m_homePage);

    m_showHome = new QCheckBox(tr("Show home button on the toolbar"), page);
    m_showHome->setChecked(AppSettings::instance()->showHomeButton());
    form->addRow(QString(), m_showHome);

    auto *dlGroup = new QGroupBox(tr("Downloads"), page);
    auto *dlForm = new QFormLayout(dlGroup);
    m_downloadDir = new QLineEdit(dlGroup);
    m_downloadDir->setText(AppSettings::instance()->downloadDir());
    auto *browse = new QPushButton(tr("Browse…"), dlGroup);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Download folder"),
                                                             AppSettings::instance()->downloadDir());
        if (!d.isEmpty())
            m_downloadDir->setText(d);
    });
    auto *dlRow = new QHBoxLayout;
    dlRow->addWidget(m_downloadDir, 1);
    dlRow->addWidget(browse);
    dlForm->addRow(tr("Save files to:"), dlRow);
    m_askSave = new QCheckBox(tr("Always ask where to save each file"), dlGroup);
    m_askSave->setChecked(AppSettings::instance()->askWhereToSave());
    dlForm->addRow(QString(), m_askSave);
    form->addRow(dlGroup);

    m_openFg = new QCheckBox(tr("Open links from other applications in a new tab"), page);
    m_openFg->setChecked(AppSettings::instance()->openLinksInForeground());
    form->addRow(QString(), m_openFg);

    m_hibernate = new QSpinBox(page);
    m_hibernate->setRange(0, 240);
    m_hibernate->setSpecialValueText(tr("Never"));
    m_hibernate->setSuffix(tr(" min"));
    m_hibernate->setValue(AppSettings::instance()->hibernateMinutes());
    form->addRow(tr("Freeze inactive tabs after:"), m_hibernate);

    return page;
}

QWidget *SettingsDialog::buildAppearancePage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_theme = new QComboBox(page);
    m_theme->addItem(tr("Follow system"), 0);
    m_theme->addItem(tr("Light"), 1);
    m_theme->addItem(tr("Dark"), 2);
    m_theme->setCurrentIndex(AppSettings::instance()->themeMode());
    form->addRow(tr("Theme:"), m_theme);

    m_accent = new QComboBox(page);
    const struct { const char *name; const char *hex; } accents[] = {
        { QT_TRANSLATE_NOOP("SettingsDialog", "Teal"),   "#0fa2a0" },
        { QT_TRANSLATE_NOOP("SettingsDialog", "Blue"),   "#3b82f6" },
        { QT_TRANSLATE_NOOP("SettingsDialog", "Violet"), "#8b5cf6" },
        { QT_TRANSLATE_NOOP("SettingsDialog", "Rose"),   "#e11d48" },
        { QT_TRANSLATE_NOOP("SettingsDialog", "Amber"),  "#d97706" },
        { QT_TRANSLATE_NOOP("SettingsDialog", "Green"),  "#16a34a" },
    };
    const QString cur = AppSettings::instance()->accentColor().toLower();
    for (const auto &a : accents) {
        m_accent->addItem(tr(a.name), QColor(a.hex).name());
        if (cur == QColor(a.hex).name())
            m_accent->setCurrentIndex(m_accent->count() - 1);
    }
    form->addRow(tr("Accent color:"), m_accent);

    m_tabPos = new QComboBox(page);
    m_tabPos->addItem(tr("Top"), 0);
    m_tabPos->addItem(tr("Bottom"), 1);
    m_tabPos->addItem(tr("Left"), 2);
    m_tabPos->setCurrentIndex(AppSettings::instance()->tabPosition());
    form->addRow(tr("Tab bar position:"), m_tabPos);

    m_showBmBar = new QCheckBox(tr("Show bookmarks bar"), page);
    m_showBmBar->setChecked(AppSettings::instance()->showBookmarksBar());
    form->addRow(QString(), m_showBmBar);

    auto *zoomRow = new QHBoxLayout;
    m_zoom = new QSpinBox(page);
    m_zoom->setRange(25, 300);
    m_zoom->setSuffix(tr("%"));
    m_zoom->setValue(AppSettings::instance()->defaultZoom());
    form->addRow(tr("Default page zoom:"), m_zoom);

    m_minFont = new QSpinBox(page);
    m_minFont->setRange(0, 24);
    m_minFont->setSpecialValueText(tr("Off"));
    m_minFont->setValue(AppSettings::instance()->minFontSize());
    form->addRow(tr("Minimum font size:"), m_minFont);

    return page;
}

QWidget *SettingsDialog::buildSearchPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_engine = new QComboBox(page);
    const QList<AppSettings::SearchEngineInfo> engines = AppSettings::searchEngines();
    const QString cur = AppSettings::instance()->searchEngine();
    for (const AppSettings::SearchEngineInfo &e : engines) {
        m_engine->addItem(e.name, e.id);
        if (e.id == cur)
            m_engine->setCurrentIndex(m_engine->count() - 1);
    }
    form->addRow(tr("Search engine used in the address bar:"), m_engine);

    m_suggest = new QCheckBox(tr("Show live search suggestions (sends keystrokes to the search engine)"), page);
    m_suggest->setChecked(AppSettings::instance()->searchSuggestions());
    form->addRow(QString(), m_suggest);

    auto *note = new QLabel(tr("<i>Search suggestions are off by default to keep your keystrokes local.</i>"), page);
    note->setWordWrap(true);
    form->addRow(QString(), note);

    return page;
}

QWidget *SettingsDialog::buildPrivacyPage()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);

    // ---- shields ----
    auto *shields = new QGroupBox(tr("Shields"), page);
    auto *sf = new QFormLayout(shields);
    m_blockAds = new QCheckBox(tr("Block advertisements (network + cosmetic filtering)"), shields);
    m_blockAds->setChecked(AppSettings::instance()->blockAds());
    sf->addRow(QString(), m_blockAds);
    m_blockTrackers = new QCheckBox(tr("Block cross-site trackers"), shields);
    m_blockTrackers->setChecked(AppSettings::instance()->blockTrackers());
    sf->addRow(QString(), m_blockTrackers);

    m_listEasyList = new QCheckBox(tr("EasyList — general ad block list"), shields);
    m_listEasyList->setChecked(AppSettings::instance()->easyListEnabled());
    sf->addRow(QString(), m_listEasyList);
    m_listEasyPrivacy = new QCheckBox(tr("EasyPrivacy — tracker block list"), shields);
    m_listEasyPrivacy->setChecked(AppSettings::instance()->easyPrivacyEnabled());
    sf->addRow(QString(), m_listEasyPrivacy);

    auto *upd = new QPushButton(tr("Reload filter lists now"), shields);
    m_rulesLoaded = new QLabel(shields);
    m_rulesLoaded->setText(tr("%L1 rules loaded").arg(PrivacyEngine::instance()->ruleCount()));
    connect(upd, &QPushButton::clicked, this, &SettingsDialog::onUpdateFilterLists);
    auto *updRow = new QHBoxLayout;
    updRow->addWidget(upd);
    updRow->addWidget(m_rulesLoaded, 1);
    sf->addRow(updRow);
    outer->addWidget(shields);

    // ---- cookies ----
    auto *cookies = new QGroupBox(tr("Cookies"), page);
    auto *cf = new QFormLayout(cookies);
    m_cookiePolicy = new QComboBox(cookies);
    m_cookiePolicy->addItem(tr("Allow all cookies"), 0);
    m_cookiePolicy->addItem(tr("Block third-party cookies (recommended)"), 1);
    m_cookiePolicy->addItem(tr("Block all cookies (breaks sites)"), 2);
    m_cookiePolicy->setCurrentIndex(AppSettings::instance()->cookiePolicy());
    cf->addRow(tr("Cookie policy:"), m_cookiePolicy);
    outer->addWidget(cookies);

    // ---- fingerprinting & identity ----
    auto *fp = new QGroupBox(tr("Fingerprinting & identity"), page);
    auto *ff = new QFormLayout(fp);
    m_fp = new QCheckBox(tr("Randomize canvas / audio / WebGL fingerprints per site"), fp);
    m_fp->setChecked(AppSettings::instance()->fingerprintProtection());
    ff->addRow(QString(), m_fp);
    m_webrtc = new QComboBox(fp);
    m_webrtc->addItem(tr("Default WebRTC behavior"), 0);
    m_webrtc->addItem(tr("Hide local IPs behind public interfaces (prevents leaks)"), 1);
    m_webrtc->addItem(tr("Disable WebRTC entirely"), 2);
    m_webrtc->setCurrentIndex(AppSettings::instance()->webrtcPolicy());
    ff->addRow(tr("WebRTC IP handling:"), m_webrtc);
    m_dnt = new QCheckBox(tr("Send “Do Not Track” (DNT) header"), fp);
    m_dnt->setChecked(AppSettings::instance()->dntHeader());
    ff->addRow(QString(), m_dnt);
    m_gpc = new QCheckBox(tr("Send Global Privacy Control (GPC) signal"), fp);
    m_gpc->setChecked(AppSettings::instance()->gpcHeader());
    ff->addRow(QString(), m_gpc);
    m_httpsFirst = new QCheckBox(tr("HTTPS-first: upgrade http:// navigations to https://"), fp);
    m_httpsFirst->setChecked(AppSettings::instance()->httpsFirst());
    ff->addRow(QString(), m_httpsFirst);
    outer->addWidget(fp);

    // ---- on-exit ----
    auto *ex = new QGroupBox(tr("On exit"), page);
    auto *ef = new QVBoxLayout(ex);
    m_exitHistory = new QCheckBox(tr("Clear browsing history"), ex);
    m_exitHistory->setChecked(AppSettings::instance()->clearHistoryOnExit());
    m_exitCookies = new QCheckBox(tr("Clear cookies and site data"), ex);
    m_exitCookies->setChecked(AppSettings::instance()->clearCookiesOnExit());
    m_exitCache = new QCheckBox(tr("Clear cached files"), ex);
    m_exitCache->setChecked(AppSettings::instance()->clearCacheOnExit());
    ef->addWidget(m_exitHistory);
    ef->addWidget(m_exitCookies);
    ef->addWidget(m_exitCache);
    outer->addWidget(ex);

    // ---- data tools ----
    auto *tools = new QGroupBox(tr("Browsing data"), page);
    auto *tf = new QVBoxLayout(tools);
    auto *clear = new QPushButton(tr("Clear browsing data…"), tools);
    connect(clear, &QPushButton::clicked, this, &SettingsDialog::onClearDataClicked);
    tf->addWidget(clear);
    auto *bmRow = new QHBoxLayout;
    auto *imp = new QPushButton(tr("Import bookmarks from JSON…"), tools);
    connect(imp, &QPushButton::clicked, this, &SettingsDialog::onImportBookmarks);
    auto *exp = new QPushButton(tr("Export bookmarks to JSON…"), tools);
    connect(exp, &QPushButton::clicked, this, &SettingsDialog::onExportBookmarks);
    bmRow->addWidget(imp);
    bmRow->addWidget(exp);
    bmRow->addStretch(1);
    tf->addLayout(bmRow);
    outer->addWidget(tools);
    outer->addStretch(1);

    return page;
}

QWidget *SettingsDialog::buildPermissionsPage()
{
    auto *page = new QWidget(this);
    auto *l = new QVBoxLayout(page);

    auto *info = new QLabel(tr("Per-site permissions already set. Double-click a cell to change between "
                               "Ask / Allow / Block. New sites use these choices as defaults."), page);
    info->setWordWrap(true);
    l->addWidget(info);

    m_permTable = new QTableWidget(page);
    m_permTable->setColumnCount(3);
    m_permTable->setHorizontalHeaderLabels({ tr("Origin"), tr("Feature"), tr("Policy") });
    m_permTable->verticalHeader()->setVisible(false);
    m_permTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_permTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_permTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_permTable, &QTableWidget::cellChanged, this, &SettingsDialog::onPermissionCellChanged);
    l->addWidget(m_permTable, 1);

    // default-permission combo for future sites
    auto *defBox = new QGroupBox(tr("Default policy for new sites"), page);
    auto *df = new QFormLayout(defBox);
    const QStringList features = {
        QStringLiteral("MediaAudioCapture"), QStringLiteral("MediaVideoCapture"),
        QStringLiteral("Geolocation"), QStringLiteral("Notifications"),
        QStringLiteral("ClipboardReadWrite"), QStringLiteral("DesktopVideoCapture"),
    };
    const QStringList readable = {
        tr("Microphone"), tr("Camera"), tr("Location"), tr("Notifications"),
        tr("Clipboard read"), tr("Screen capture"),
    };
    for (int i = 0; i < features.size(); ++i) {
        auto *cb = new QComboBox(defBox);
        cb->addItem(tr("Ask"), 0);
        cb->addItem(tr("Allow"), 1);
        cb->addItem(tr("Block"), 2);
        cb->setCurrentIndex(AppSettings::instance()->permissionDefault(features[i]));
        const QString f = features[i];
        connect(cb, &QComboBox::currentIndexChanged, this, [f, cb](int) {
            AppSettings::instance()->setPermissionDefault(f, cb->currentData().toInt());
        });
        df->addRow(readable[i], cb);
    }
    l->addWidget(defBox);

    auto *rm = new QPushButton(tr("Remove selected exception"), page);
    connect(rm, &QPushButton::clicked, this, &SettingsDialog::onPermissionRemove);
    l->addWidget(rm, 0, Qt::AlignRight);

    return page;
}

QWidget *SettingsDialog::buildAdvancedPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *uaBox = new QGroupBox(tr("User agent"), page);
    auto *uf = new QFormLayout(uaBox);
    m_ua = new QComboBox(uaBox);
    m_ua->addItem(tr("Default (identify as WED)"), 0);
    m_ua->addItem(tr("Chrome (Windows)"), 1);
    m_ua->addItem(tr("Firefox (Windows)"), 2);
    m_ua->addItem(tr("Custom…"), 3);
    m_ua->setCurrentIndex(AppSettings::instance()->userAgentMode());
    m_uaCustom = new QLineEdit(uaBox);
    m_uaCustom->setPlaceholderText(tr("Mozilla/5.0 …"));
    m_uaCustom->setText(AppSettings::instance()->customUserAgent());
    uf->addRow(tr("Mode:"), m_ua);
    uf->addRow(tr("Custom string:"), m_uaCustom);
    form->addRow(uaBox);

    auto *pxBox = new QGroupBox(tr("Network proxy"), page);
    auto *pf = new QFormLayout(pxBox);
    m_proxy = new QComboBox(pxBox);
    m_proxy->addItem(tr("Use system proxy"), 0);
    m_proxy->addItem(tr("No proxy"), 1);
    m_proxy->addItem(tr("Manual HTTP proxy"), 2);
    m_proxy->setCurrentIndex(AppSettings::instance()->proxyMode());
    m_proxyHost = new QLineEdit(pxBox);
    m_proxyHost->setText(AppSettings::instance()->proxyHost());
    m_proxyHost->setPlaceholderText(tr("proxy.example.com"));
    m_proxyPort = new QSpinBox(pxBox);
    m_proxyPort->setRange(1, 65535);
    m_proxyPort->setValue(AppSettings::instance()->proxyPort());
    pf->addRow(tr("Mode:"), m_proxy);
    pf->addRow(tr("Host:"), m_proxyHost);
    pf->addRow(tr("Port:"), m_proxyPort);
    form->addRow(pxBox);

    m_hwAccel = new QCheckBox(tr("Hardware acceleration (GPU compositing)"), page);
    m_hwAccel->setChecked(AppSettings::instance()->hardwareAcceleration());
    form->addRow(QString(), m_hwAccel);

    form->addRow(new QLabel(tr("<i>Changes to the user agent, proxy and GPU settings fully apply "
                               "after restarting WED.</i>"), page));
    return page;
}

void SettingsDialog::loadPermissionExceptions()
{
    const QList<Database::PermissionRow> rows = Database::instance()->allPermissions();
    m_permTable->setRowCount(rows.size());
    m_permTable->blockSignals(true);
    for (int i = 0; i < rows.size(); ++i) {
        auto *o = new QTableWidgetItem(rows[i].origin);
        auto *f = new QTableWidgetItem(rows[i].feature);
        auto *cb = new QComboBox;  // editor widget embedded via cell widget
        cb->addItem(tr("Ask"), 0);
        cb->addItem(tr("Allow"), 1);
        cb->addItem(tr("Block"), 2);
        cb->setProperty("row", i);
        cb->setCurrentIndex(rows[i].policy);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, o, f, cb](int) {
            Database::instance()->setPermission(o->text(), f->text(), cb->currentData().toInt());
        });
        m_permTable->setItem(i, 0, o);
        m_permTable->setItem(i, 1, f);
        m_permTable->setCellWidget(i, 2, cb);
        o->setFlags(o->flags() & ~Qt::ItemIsEditable);
        f->setFlags(f->flags() & ~Qt::ItemIsEditable);
    }
    m_permTable->blockSignals(false);
}

void SettingsDialog::onPermissionCellChanged(int, int)
{
    // policies edited through embedded combos; kept for future inline editing
}

void SettingsDialog::onPermissionRemove()
{
    const int row = m_permTable->currentRow();
    if (row < 0)
        return;
    const QString origin = m_permTable->item(row, 0)->text();
    const QString feature = m_permTable->item(row, 1)->text();
    Database::instance()->removePermission(origin, feature);
    loadPermissionExceptions();
}

void SettingsDialog::onUpdateFilterLists()
{
    PrivacyEngine::instance()->reload();
    m_rulesLoaded->setText(tr("%L1 rules loaded").arg(PrivacyEngine::instance()->ruleCount()));
}

void SettingsDialog::onClearDataClicked()
{
    ClearBrowsingDataDialog dlg(this);
    dlg.exec();
}

void SettingsDialog::onImportBookmarks()
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Import bookmarks"), QString(),
                                                    tr("JSON (*.json);;All files (*)"));
    if (f.isEmpty())
        return;
    QFile file(f);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import failed"), tr("Could not open %1.").arg(f));
        return;
    }
    const int n = Database::instance()->importBookmarksJson(QString::fromUtf8(file.readAll()));
    file.close();
    QMessageBox::information(this, tr("Import finished"),
                             n >= 0 ? tr("Imported %n bookmark(s).", "", n)
                                    : tr("The file is not a valid bookmarks export."));
}

void SettingsDialog::onExportBookmarks()
{
    const QString f = QFileDialog::getSaveFileName(this, tr("Export bookmarks"), QString("bookmarks.json"),
                                                     tr("JSON (*.json)"));
    if (f.isEmpty())
        return;
    QFile file(f);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not write %1.").arg(f));
        return;
    }
    file.write(Database::instance()->exportBookmarksJson().toUtf8());
    file.close();
}

void SettingsDialog::accept()
{
    AppSettings *s = AppSettings::instance();

    // general
    s->setStartupMode(m_startupMode->currentData().toInt());
    QStringList urls = m_startupUrls->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &u : urls) u = u.trimmed();
    urls.removeAll(QString());
    s->setStartupUrls(urls);
    s->setHomePage(m_homePage->text().trimmed());
    s->setShowHomeButton(m_showHome->isChecked());
    s->setDownloadDir(m_downloadDir->text().trimmed());
    s->setAskWhereToSave(m_askSave->isChecked());
    s->setOpenLinksInForeground(m_openFg->isChecked());
    s->setHibernateMinutes(m_hibernate->value());

    // appearance
    const int oldTheme = s->themeMode();
    const QString oldAccent = s->accentColor();
    s->setThemeMode(m_theme->currentData().toInt());
    s->setAccentColor(m_accent->currentData().toString());
    s->setTabPosition(m_tabPos->currentData().toInt());
    s->setShowBookmarksBar(m_showBmBar->isChecked());
    s->setDefaultZoom(m_zoom->value());
    s->setMinFontSize(m_minFont->value());
    if (s->themeMode() != oldTheme || s->accentColor() != oldAccent)
        ThemeManager::instance()->apply();

    // search
    s->setSearchEngine(m_engine->currentData().toString());
    s->setSearchSuggestions(m_suggest->isChecked());

    // privacy
    s->setBlockAds(m_blockAds->isChecked());
    s->setBlockTrackers(m_blockTrackers->isChecked());
    s->setCookiePolicy(m_cookiePolicy->currentData().toInt());
    s->setDntHeader(m_dnt->isChecked());
    s->setGpcHeader(m_gpc->isChecked());
    s->setFingerprintProtection(m_fp->isChecked());
    s->setHttpsFirst(m_httpsFirst->isChecked());
    s->setWebRtcPolicy(m_webrtc->currentData().toInt());
    const bool listsChanged = s->easyListEnabled() != m_listEasyList->isChecked()
                           || s->easyPrivacyEnabled() != m_listEasyPrivacy->isChecked();
    s->setEasyListEnabled(m_listEasyList->isChecked());
    s->setEasyPrivacyEnabled(m_listEasyPrivacy->isChecked());
    if (listsChanged)
        PrivacyEngine::instance()->reload();
    s->setClearHistoryOnExit(m_exitHistory->isChecked());
    s->setClearCookiesOnExit(m_exitCookies->isChecked());
    s->setClearCacheOnExit(m_exitCache->isChecked());

    // advanced
    s->setUserAgentMode(m_ua->currentData().toInt());
    s->setCustomUserAgent(m_uaCustom->text().trimmed());
    s->setProxyMode(m_proxy->currentData().toInt());
    s->setProxyHost(m_proxyHost->text().trimmed());
    s->setProxyPort(m_proxyPort->value());
    s->setHardwareAcceleration(m_hwAccel->isChecked());

    ProfileCatalog::instance()->applyUserAgent();
    ProfileCatalog::instance()->applyFingerprintScript();
    ProfileCatalog::instance()->reloadPrivacyPolicies();

    s->sync();
    QDialog::accept();
}
