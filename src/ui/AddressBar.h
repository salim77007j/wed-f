#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QListView>
#include <QStandardItemModel>
#include <QNetworkAccessManager>

class QToolButton;
class BrowserTab;

// Security indicator state shown in the address field
enum class PageSecurity {
    Unknown = 0,
    Secure,        // https, valid cert
    Insecure,      // http
    CertWarning,   // overridable cert error this session
    CertBlocked,
};

// The composite address bar: security chip + input + shield + star,
// suggestion popup merging history / bookmarks / search engines / web suggest.
class AddressBar : public QWidget
{
    Q_OBJECT
public:
    explicit AddressBar(QWidget *parent = nullptr);

    void setTab(BrowserTab *tab);
    QLineEdit *editor() const { return m_edit; }
    void setPageSecurity(PageSecurity s);
    void setFavicon(const QIcon &icon);
    void setShieldCount(int ads, int trackers, int cookies);
    void setBookmarked(bool b);
    void updateDisplayForTab();      // sync text/icons with tab state
    void focusAndSelectAll();
    bool isSuggestionPopupVisible() const;

    // convert user input to a navigable URL or a search
    static QUrl inputToUrl(const QString &text);
    static QString searchUrlFor(const QString &text);

signals:
    void navigateRequested(const QUrl &url);
    void searchRequested(const QString &text);
    void shieldClicked();
    void starClicked();
    void siteInfoRequested();
    void focusMovedOut();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onReturnPressed();
    void onTextChanged(const QString &text);
    void onSuggestionActivated(const QModelIndex &idx);
    void onSuggestionHighlighted(const QModelIndex &idx);
    void fetchWebSuggestions(const QString &text);

private:
    void buildSuggestionModel(const QString &text);
    void showPopup();
    void hidePopup();
    void moveSuggestion(int delta);
    void styleSecurityChip();

    QLineEdit *m_edit = nullptr;
    QToolButton *m_secButton = nullptr;
    QToolButton *m_shield = nullptr;
    QToolButton *m_star = nullptr;
    QFrame *m_popup = nullptr;
    QListView *m_popupView = nullptr;
    QStandardItemModel *m_model = nullptr;
    BrowserTab *m_tab = nullptr;
    PageSecurity m_security = PageSecurity::Unknown;
    bool m_bookmarked = false;
    bool m_selectAllOnNextFocus = false;
    QNetworkAccessManager m_nam;
    bool m_suggestReplyPending = false;
    QString m_suggestFor;
    QIcon m_favicon;
};
