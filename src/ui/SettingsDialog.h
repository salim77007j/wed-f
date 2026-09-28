#pragma once
#include <QDialog>
#include <QTabWidget>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QLineEdit;
class QTableWidget;
class QButtonGroup;
class QLabel;

// Clear-browsing-data dialog (also used from the Tools menu): real deletions
// for history, cookies, cache, site stats and closed-tabs list.
class ClearBrowsingDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ClearBrowsingDataDialog(QWidget *parent = nullptr);
private slots:
    void onClear();
private:
    QCheckBox *m_history = nullptr;
    QCheckBox *m_cookies = nullptr;
    QCheckBox *m_cache = nullptr;
    QCheckBox *m_stats = nullptr;
    QCheckBox *m_closedTabs = nullptr;
};

// Deep settings dialog. Pages: General, Appearance, Search, Privacy & Security,
// Site Permissions, Advanced. All controls read/write AppSettings and apply live.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    enum Page { PageGeneral = 0, PageAppearance, PageSearch, PagePrivacy, PagePermissions, PageAdvanced };
    explicit SettingsDialog(QWidget *parent = nullptr);
    void setCurrentPage(int page);

private slots:
    void accept() override;
    void onClearDataClicked();
    void onPermissionCellChanged(int row, int col);
    void onPermissionRemove();
    void onUpdateFilterLists();
    void onImportBookmarks();
    void onExportBookmarks();

private:
    QWidget *buildGeneralPage();
    QWidget *buildAppearancePage();
    QWidget *buildSearchPage();
    QWidget *buildPrivacyPage();
    QWidget *buildPermissionsPage();
    QWidget *buildAdvancedPage();
    void loadPermissionExceptions();

    QTabWidget *m_tabs = nullptr;

    // general
    QComboBox *m_startupMode = nullptr;
    QLineEdit *m_startupUrls = nullptr;
    QLineEdit *m_homePage = nullptr;
    QCheckBox *m_showHome = nullptr;
    QLineEdit *m_downloadDir = nullptr;
    QCheckBox *m_askSave = nullptr;
    QCheckBox *m_openFg = nullptr;
    QSpinBox *m_hibernate = nullptr;

    // appearance
    QComboBox *m_theme = nullptr;
    QComboBox *m_accent = nullptr;
    QComboBox *m_tabPos = nullptr;
    QCheckBox *m_showBmBar = nullptr;
    QSpinBox *m_zoom = nullptr;
    QSpinBox *m_minFont = nullptr;

    // search
    QComboBox *m_engine = nullptr;
    QCheckBox *m_suggest = nullptr;

    // privacy
    QCheckBox *m_blockAds = nullptr;
    QCheckBox *m_blockTrackers = nullptr;
    QComboBox *m_cookiePolicy = nullptr;
    QCheckBox *m_dnt = nullptr;
    QCheckBox *m_gpc = nullptr;
    QCheckBox *m_fp = nullptr;
    QCheckBox *m_httpsFirst = nullptr;
    QComboBox *m_webrtc = nullptr;
    QCheckBox *m_listEasyList = nullptr;
    QCheckBox *m_listEasyPrivacy = nullptr;
    QLabel *m_rulesLoaded = nullptr;
    QCheckBox *m_exitHistory = nullptr;
    QCheckBox *m_exitCookies = nullptr;
    QCheckBox *m_exitCache = nullptr;

    // permissions
    QTableWidget *m_permTable = nullptr;

    // advanced
    QComboBox *m_ua = nullptr;
    QLineEdit *m_uaCustom = nullptr;
    QComboBox *m_proxy = nullptr;
    QLineEdit *m_proxyHost = nullptr;
    QSpinBox *m_proxyPort = nullptr;
    QCheckBox *m_hwAccel = nullptr;
};
