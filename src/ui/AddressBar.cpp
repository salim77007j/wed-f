#include "AddressBar.h"
#include "AppSettings.h"
#include "BrowserTab.h"
#include "Database.h"
#include "ThemeManager.h"
#include "Utils.h"

#include <QApplication>
#include <QCompleter>

#include <QEvent>
#include <QFocusEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

// roles for suggestion items
enum SuggestKind {
    KindHistory = Qt::UserRole + 1,
    KindBookmark,
    KindSearch,
    KindWebSuggest,
    KindUrl,
};
enum SuggestData { RoleKind = Qt::UserRole + 9, RoleUrl = Qt::UserRole + 10, RoleInputText };

AddressBar::AddressBar(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 2, 6, 2);
    lay->setSpacing(4);

    m_secButton = new QToolButton(this);
    m_secButton->setObjectName("secChip");
    m_secButton->setCursor(Qt::PointingHandCursor);
    m_secButton->setToolTip(tr("Site information"));
    m_secButton->setIcon(Icons::themed("info"));
    m_secButton->setAutoRaise(true);
    connect(m_secButton, &QToolButton::clicked, this, &AddressBar::siteInfoRequested);
    lay->addWidget(m_secButton);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Search the web or enter address"));
    m_edit->setClearButtonEnabled(false);
    m_edit->setFrame(false);
    m_edit->installEventFilter(this);
    m_edit->setContextMenuPolicy(Qt::DefaultContextMenu);
    lay->addWidget(m_edit, 1);
    connect(m_edit, &QLineEdit::returnPressed, this, &AddressBar::onReturnPressed);
    connect(m_edit, &QLineEdit::textEdited, this, &AddressBar::onTextChanged);

    m_shield = new QToolButton(this);
    m_shield->setCursor(Qt::PointingHandCursor);
    m_shield->setToolTip(tr("Protections: nothing blocked yet"));
    m_shield->setIcon(Icons::themed("shield"));
    m_shield->setAutoRaise(true);
    connect(m_shield, &QToolButton::clicked, this, &AddressBar::shieldClicked);
    lay->addWidget(m_shield);

    m_star = new QToolButton(this);
    m_star->setCursor(Qt::PointingHandCursor);
    m_star->setToolTip(tr("Bookmark this page"));
    m_star->setIcon(Icons::themed("star"));
    m_star->setAutoRaise(true);
    connect(m_star, &QToolButton::clicked, this, &AddressBar::starClicked);
    lay->addWidget(m_star);

    // suggestion popup
    m_popup = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_popup->setAttribute(Qt::WA_ShowWithoutActivating);
    auto *popLay = new QVBoxLayout(m_popup);
    popLay->setContentsMargins(0, 0, 0, 0);
    m_popupView = new QListView(m_popup);
    m_popupView->setWindowFlag(Qt::SubWindow);
    m_popupView->setUniformItemSizes(true);
    m_popupView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_popupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popupView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_popupView->setFocusPolicy(Qt::NoFocus);
    m_model = new QStandardItemModel(this);
    m_popupView->setModel(m_model);
    popLay->addWidget(m_popupView);
    connect(m_popupView, &QListView::clicked, this, &AddressBar::onSuggestionActivated);
    connect(m_popupView, &QListView::activated, this, &AddressBar::onSuggestionActivated);
    connect(m_popupView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &AddressBar::onSuggestionHighlighted);
}

void AddressBar::setTab(BrowserTab *tab) { m_tab = tab; }

void AddressBar::focusAndSelectAll()
{
    m_edit->setFocus();
    m_edit->selectAll();
}

bool AddressBar::isSuggestionPopupVisible() const { return m_popup->isVisible(); }

void AddressBar::setPageSecurity(PageSecurity s)
{
    if (m_security == s) return;
    m_security = s;
    styleSecurityChip();
}

void AddressBar::styleSecurityChip()
{
    switch (m_security) {
    case PageSecurity::Secure:
        m_secButton->setIcon(Icons::make("lock", QColor("#188038")));
        m_secButton->setToolTip(tr("Connection is secure (HTTPS)"));
        break;
    case PageSecurity::Insecure:
        m_secButton->setIcon(Icons::make("info", ThemeManager::instance()->color(RoleTextSecondary)));
        m_secButton->setToolTip(tr("Connection is not secure (HTTP)"));
        break;
    case PageSecurity::CertWarning:
        m_secButton->setIcon(Icons::make("warn", QColor("#e37400")));
        m_secButton->setToolTip(tr("Certificate warning — connection may be unsafe"));
        break;
    case PageSecurity::CertBlocked:
        m_secButton->setIcon(Icons::make("warn", QColor("#d93025")));
        m_secButton->setToolTip(tr("Certificate rejected — connection blocked"));
        break;
    default:
        m_secButton->setIcon(Icons::themed("info"));
        m_secButton->setToolTip(tr("Site information"));
    }
}

void AddressBar::setFavicon(const QIcon &icon)
{
    m_favicon = icon;
    if (m_security == PageSecurity::Unknown) {
        // show favicon while security unknown? keep chip icon
    }
}

void AddressBar::setShieldCount(int ads, int trackers, int cookies)
{
    int total = ads + trackers;
    const bool shieldsOff = (ads == -1);
    if (shieldsOff) {
        m_shield->setIcon(Icons::themed("shield-off"));
        m_shield->setToolTip(tr("Protections are OFF for this site"));
        return;
    }
    if (total > 0 || cookies > 0) {
        m_shield->setIcon(Icons::make("shield", ThemeManager::instance()->accent()));
        m_shield->setToolTip(tr("%1 ads and %2 trackers blocked on this page\n%3 third-party cookies blocked")
                                 .arg(ads).arg(trackers).arg(cookies));
    } else {
        m_shield->setIcon(Icons::themed("shield"));
        m_shield->setToolTip(tr("Protections active — nothing blocked on this page"));
    }
}

void AddressBar::setBookmarked(bool b)
{
    m_bookmarked = b;
    m_star->setIcon(b ? Icons::make("star-filled", ThemeManager::instance()->accent())
                      : Icons::themed("star"));
    m_star->setToolTip(b ? tr("Edit bookmark") : tr("Bookmark this page"));
}

void AddressBar::updateDisplayForTab()
{
    if (!m_tab) return;
    const QUrl u = m_tab->currentUrl();
    QString text = u.toString();
    if (u.scheme() == "wed" || u.toString() == "about:blank") text.clear();
    if (m_edit->text() != text && !m_edit->hasFocus()) {
        m_edit->setText(text);
        m_edit->setCursorPosition(0);
    }
    // security
    if (u.scheme() == "https") {
        WebPage *p = m_tab->page();
        if (p && p->hadCertError())
            setPageSecurity(PageSecurity::CertWarning);
        else
            setPageSecurity(PageSecurity::Secure);
    } else if (u.scheme() == "http") {
        setPageSecurity(PageSecurity::Insecure);
    } else {
        setPageSecurity(PageSecurity::Unknown);
    }
    setBookmarked(Database::instance()->isBookmarked(u.toString()));
}

// ---------------------------------------------------------------- input → url

QUrl AddressBar::inputToUrl(const QString &textIn)
{
    QString text = textIn.trimmed();
    if (text.isEmpty()) return QUrl();

    // internal pages
    if (text == "wed://start" || text == "wed://home" || text == "newtab")
        return QUrl("wed://start");
    if (text.startsWith("about:"))
        return QUrl(text);

    // contains spaces → search
    if (text.contains(' '))
        return QUrl();

    // search engine keyword: "g cats", "w berlin"
    const QStringList words = text.split(' ', Qt::SkipEmptyParts);
    for (const auto &e : AppSettings::searchEngines()) {
        if (words.size() >= 2 && words.first() == e.id.left(1) && text.contains(' ')) {
            QString q = text.mid(text.indexOf(' ') + 1);
            return QUrl(e.searchUrl.arg(QString(QUrl::toPercentEncoding(q))));
        }
    }

    // explicit scheme
    if (text.startsWith("http://") || text.startsWith("https://") || text.startsWith("file://")
        || text.startsWith("localhost") || text.contains("://"))
        return QUrl::fromUserInput(text);

    // host-looking: dot with valid TLD-ish suffix, no spaces
    static const QRegularExpression hostRe(
        R"(^[\w-]+(\.[\w-]+)+(:\d+)?(/.*)?$)", QRegularExpression::CaseInsensitiveOption);
    if (hostRe.match(text).hasMatch())
        return QUrl::fromUserInput(text);

    // ip address
    static const QRegularExpression ipRe(R"(^\d{1,3}(\.\d{1,3}){3}(:\d+)?(/.*)?$)");
    if (ipRe.match(text).hasMatch())
        return QUrl::fromUserInput("http://" + text);

    return QUrl(); // → search
}

QString AddressBar::searchUrlFor(const QString &text)
{
    const QString id = AppSettings::instance()->searchEngine();
    for (const auto &e : AppSettings::searchEngines()) {
        if (e.id == id)
            return e.searchUrl.arg(QString(QUrl::toPercentEncoding(text.trimmed())));
    }
    return QStringLiteral("https://duckduckgo.com/?q=%1")
        .arg(QString(QUrl::toPercentEncoding(text.trimmed())));
}

void AddressBar::onReturnPressed()
{
    if (isSuggestionPopupVisible()) {
        const QModelIndex idx = m_popupView->currentIndex();
        if (idx.isValid()) {
            onSuggestionActivated(idx);
            return;
        }
    }
    const QUrl u = inputToUrl(m_edit->text());
    if (u.isEmpty())
        emit searchRequested(m_edit->text().trimmed());
    else
        emit navigateRequested(u);
    hidePopup();
}

// ---------------------------------------------------------------- suggestions

void AddressBar::onTextChanged(const QString &text)
{
    if (text.trimmed().size() < 2) {
        hidePopup();
        return;
    }
    buildSuggestionModel(text);
    showPopup();
    if (AppSettings::instance()->searchSuggestions())
        fetchWebSuggestions(text);
}

void AddressBar::buildSuggestionModel(const QString &text)
{
    m_model->clear();
    const QString t = text.trimmed();

    // 1. direct URL / search header
    const QUrl direct = inputToUrl(t);
    if (!direct.isEmpty()) {
        QStandardItem *it = new QStandardItem(Icons::themed("globe"), direct.toString());
        it->setData(KindUrl, RoleKind);
        it->setData(direct.toString(), RoleUrl);
        m_model->appendRow(it);
    } else {
        QString engine = QStringLiteral("DuckDuckGo");
        for (const auto &e : AppSettings::searchEngines())
            if (e.id == AppSettings::instance()->searchEngine()) engine = e.name;
        QStandardItem *it = new QStandardItem(Icons::themed("search"),
                                              tr("Search %1 for \"%2\"").arg(engine, t));
        it->setData(KindSearch, RoleKind);
        it->setData(t, RoleUrl);
        m_model->appendRow(it);
    }

    // 2. history (max 6)
    int h = 0;
    for (const Database::HistoryEntry &e : Database::instance()->searchHistory(t, 30)) {
        if (e.url == t) continue;
        if (h++ >= 6) break;
        QStandardItem *it = new QStandardItem(Icons::themed("history"),
                                              e.title.isEmpty() ? e.url : e.title);
        it->setData(e.url, RoleUrl);
        it->setData(KindHistory, RoleKind);
        QStandardItem *sub = new QStandardItem(e.url);
        sub->setData(e.url, RoleUrl);
        it->setChild(0, sub);
        it->setToolTip(e.url);
        m_model->appendRow(it);
    }

    // 3. bookmarks (max 4)
    int b = 0;
    for (const Database::BookmarkNode &n : Database::instance()->allBookmarks()) {
        if (n.folder || n.url.isEmpty()) continue;
        if (!(n.url.contains(t, Qt::CaseInsensitive) || n.title.contains(t, Qt::CaseInsensitive)))
            continue;
        if (b++ >= 4) break;
        QStandardItem *it = new QStandardItem(Icons::make("star-filled", ThemeManager::instance()->accent()),
                                              n.title.isEmpty() ? n.url : n.title);
        it->setData(n.url, RoleUrl);
        it->setData(KindBookmark, RoleKind);
        it->setToolTip(n.url);
        m_model->appendRow(it);
    }
    m_popupView->setModelColumn(0);
}

void AddressBar::fetchWebSuggestions(const QString &text)
{
    if (m_suggestReplyPending) return;
    const QString id = AppSettings::instance()->searchEngine();
    QString tpl;
    for (const auto &e : AppSettings::searchEngines())
        if (e.id == id) tpl = e.suggestionUrl;
    if (tpl.isEmpty()) return;

    m_suggestFor = text;
    m_suggestReplyPending = true;
    QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(tpl.arg(QString(QUrl::toPercentEncoding(text.trimmed()))))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_suggestReplyPending = false;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QByteArray body = reply->readAll();
        // formats: ["query",["a","b"]] or ["query",["a"],[".."]] (duckduckgo list)
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray()) return;
        const QJsonArray arr = doc.array();
        if (arr.size() < 2 || !arr.at(1).isArray()) return;
        const QJsonArray words = arr.at(1).toArray();
        int added = 0;
        // insert right after the first row (search/url header)
        int row = 1;
        for (const QJsonValue &v : words) {
            const QString s = v.toString();
            if (s.isEmpty() || s.compare(m_suggestFor, Qt::CaseInsensitive) == 0) continue;
            if (added++ >= 6) break;
            QStandardItem *it = new QStandardItem(Icons::themed("search"), s);
            it->setData(s, RoleUrl);
            it->setData(KindWebSuggest, RoleKind);
            m_model->insertRow(row++, it);
        }
        if (added && m_popup->isVisible())
            m_popupView->reset();
    });
}

void AddressBar::showPopup()
{
    if (m_model->rowCount() == 0) {
        hidePopup();
        return;
    }
    m_popupView->setCurrentIndex(m_model->index(0, 0));
    // size & position below the editor
    const int w = qMax(width(), 420);
    int h = qMin(320, m_model->rowCount() * 34 + 8);
    m_popup->setFixedSize(w, h);
    QPoint pos = m_edit->mapToGlobal(QPoint(-6, m_edit->height() + 2));
    m_popup->move(pos);
    if (!m_popup->isVisible())
        m_popup->show();
}

void AddressBar::hidePopup()
{
    m_popup->hide();
}

void AddressBar::moveSuggestion(int delta)
{
    if (!m_popup->isVisible()) return;
    const int rows = m_model->rowCount();
    int next = m_popupView->currentIndex().row() + delta;
    if (next < 0) next = 0;
    if (next >= rows) next = rows - 1;
    m_popupView->setCurrentIndex(m_model->index(next, 0));
    onSuggestionHighlighted(m_model->index(next, 0));
}

void AddressBar::onSuggestionActivated(const QModelIndex &idx)
{
    if (!idx.isValid()) return;
    const int kind = idx.data(RoleKind).toInt();
    const QString data = idx.data(RoleUrl).toString();
    hidePopup();
    if (kind == KindSearch) {
        emit searchRequested(data);
    } else {
        const QUrl u(data);
        if (u.isValid() && !u.isEmpty())
            emit navigateRequested(u);
    }
    m_edit->clearFocus();
}

void AddressBar::onSuggestionHighlighted(const QModelIndex &idx)
{
    if (!idx.isValid()) return;
    const int kind = idx.data(RoleKind).toInt();
    if (kind == KindSearch) return;
    m_edit->setText(idx.data(RoleUrl).toString());
}

bool AddressBar::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_edit) {
        if (ev->type() == QEvent::FocusOut) {
            QTimer::singleShot(120, this, [this] {
                if (!m_popup->underMouse()) hidePopup();
            });
            updateDisplayForTab();
        } else if (ev->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(ev);
            if (ke->key() == Qt::Key_Down) { moveSuggestion(1); return true; }
            if (ke->key() == Qt::Key_Up) { moveSuggestion(-1); return true; }
            if (ke->key() == Qt::Key_Escape) {
                hidePopup();
                updateDisplayForTab();
                m_edit->clearFocus();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}
