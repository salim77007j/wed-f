#include "StartPage.h"
#include "AddressBar.h"
#include "AppSettings.h"
#include "Database.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
class DialTile : public QFrame
{
    Q_OBJECT
public:
    DialTile(const QString &title, const QUrl &url, QWidget *parent = nullptr)
        : QFrame(parent), m_url(url)
    {
        setFixedSize(150, 110);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, true);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 10, 10, 8);
        lay->setSpacing(6);

        QString host = Utils::hostOfUrl(url);
        QLabel *badge = new QLabel(this);
        badge->setFixedSize(44, 44);
        badge->setAlignment(Qt::AlignCenter);
        QFont f = badge->font();
        f.setBold(true);
        f.setPixelSize(17);
        f.setCapitalization(QFont::AllUppercase);
        badge->setFont(f);
        badge->setText(host.left(2).toUpper());
        badge->setStyleSheet(QStringLiteral(
            "border-radius:22px; background:%1; color:%2; font-weight:700;")
            .arg(ThemeManager::instance()->accent().name(),
                 QColor(Qt::white).name()));
        lay->addWidget(badge, 0, Qt::AlignHCenter);

        QLabel *name = new QLabel(title.isEmpty() ? host : title, this);
        name->setWordWrap(true);
        name->setAlignment(Qt::AlignHCenter);
        name->setStyleSheet(QStringLiteral("font-size:12px; color:%1; border:none; background:transparent;")
                                .arg(ThemeManager::instance()->color(RoleTextSecondary).name()));
        lay->addWidget(name, 1, Qt::AlignTop);
        setToolTip(url.toString());
        restyle();
    }
    QUrl url() const { return m_url; }
    void restyle()
    {
        const bool dark = ThemeManager::instance()->isDark();
        setStyleSheet(QStringLiteral(
            "DialTile { background:%1; border:1px solid %2; border-radius:12px; }"
            "DialTile[hover=\"true\"] { background:%3; border-color:%4; }")
            .arg(dark ? QColor("#26272b").name() : QColor("#ffffff").name(),
                 dark ? QColor("#333437").name() : QColor("#e4e7ea").name(),
                 dark ? QColor("#2f3136").name() : QColor("#f1f3f4").name(),
                 ThemeManager::instance()->accent().name()));
    }
protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            emit clicked(m_url);
        QFrame::mousePressEvent(e);
    }
    void enterEvent(QEnterEvent *e) override
    {
        setProperty("hover", true);
        style()->unpolish(this);
        style()->polish(this);
        QFrame::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        setProperty("hover", false);
        style()->unpolish(this);
        style()->polish(this);
        QFrame::leaveEvent(e);
    }
signals:
    void clicked(const QUrl &url);
private:
    QUrl m_url;
};
} // namespace

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_StyledBackground, false);
    buildUi();
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this] {
        rebuild();
    });
}

void StartPage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    auto *center = new QWidget(this);
    center->setFixedWidth(760);
    auto *lay = new QVBoxLayout(center);
    lay->setSpacing(18);
    lay->setContentsMargins(0, 40, 0, 40);

    // logo
    auto *logo = new QLabel("WED", this);
    QFont logoFont = logo->font();
    logoFont.setPixelSize(44);
    logoFont.setBold(true);
    logoFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    logo->setFont(logoFont);
    logo->setAlignment(Qt::AlignHCenter);
    logo->setStyleSheet(QStringLiteral("color:%1; border:none; background:transparent;")
                            .arg(ThemeManager::instance()->accent().name()));
    lay->addWidget(logo);

    auto *tagline = new QLabel(tr("Fast. Private. Yours."), this);
    tagline->setAlignment(Qt::AlignHCenter);
    tagline->setStyleSheet(QStringLiteral("color:%1; border:none; background:transparent; font-size:13px;")
                               .arg(ThemeManager::instance()->color(RoleTextSecondary).name()));
    lay->addWidget(tagline);
    lay->addSpacing(6);

    // search box
    auto *searchWrap = new QWidget(this);
    auto *sl = new QHBoxLayout(searchWrap);
    sl->setContentsMargins(14, 9, 14, 9);
    m_search = new QLineEdit(searchWrap);
    m_search->setPlaceholderText(tr("Search the web"));
    m_search->setFrame(false);
    m_search->setFixedHeight(30);
    sl->addWidget(m_search, 1);
    searchWrap->setFixedWidth(560);
    searchWrap->setStyleSheet(QStringLiteral(
        "QWidget#startSearchWrap, QWidget { background:%1; border:1px solid %2; border-radius:22px; }"
        "QLineEdit { background:transparent; border:none; font-size:15px; color:%3; }")
        .arg(ThemeManager::instance()->color(RoleInputBg).name(),
             ThemeManager::instance()->color(RoleBorder).name(),
             ThemeManager::instance()->color(RoleText).name()));
    lay->addWidget(searchWrap, 0, Qt::AlignHCenter);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        const QString t = m_search->text().trimmed();
        if (t.isEmpty()) return;
        const QUrl u = AddressBar::inputToUrl(t);
        emit navigateRequested(u.isEmpty() ? QUrl(AddressBar::searchUrlFor(t)) : u);
    });

    // speed dial
    m_dialHost = new QWidget(this);
    m_dialHost->setStyleSheet("border:none; background:transparent;");
    lay->addWidget(m_dialHost, 0, Qt::AlignHCenter);

    // stats card
    m_statsLabel = new QLabel(this);
    m_statsLabel->setAlignment(Qt::AlignHCenter);
    m_statsLabel->setStyleSheet(QStringLiteral(
        "color:%1; border:none; background:transparent; font-size:12px;")
        .arg(ThemeManager::instance()->color(RoleTextSecondary).name()));
    lay->addSpacing(4);
    lay->addWidget(m_statsLabel);

    outer->addWidget(center);
    refresh();
}

void StartPage::rebuild()
{
    // tear down children + layout, rebuild with fresh palette
    if (layout())
        delete layout();
    const auto kids = findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
    for (QWidget *w : kids)
        delete w;
    m_dialHost = nullptr;
    m_search = nullptr;
    m_statsLabel = nullptr;
    buildUi();
    update();
}

void StartPage::refresh()
{
    if (!m_dialHost)
        return;
    // clear dial
    if (QLayout *old = m_dialHost->layout())
        delete old;
    const QList<Database::HistoryEntry> top = Database::instance()->topSites(8);
    auto *gl = new QGridLayout(m_dialHost);
    gl->setContentsMargins(0, 8, 0, 8);
    gl->setSpacing(14);
    int col = 0, row = 0;
    for (const Database::HistoryEntry &e : top) {
        if (e.url.startsWith("wed://") || e.host.isEmpty()) continue;
        auto *tile = new DialTile(e.title, QUrl(e.url), m_dialHost);
        connect(tile, &DialTile::clicked, this, [this](const QUrl &u) {
            emit navigateRequested(u);
        });
        gl->addWidget(tile, row, col);
        if (++col == 4) { col = 0; ++row; }
    }
    if (top.isEmpty()) {
        auto *hint = new QLabel(tr("Your most visited sites will appear here"), m_dialHost);
        hint->setStyleSheet("color:#9aa0a6; border:none; background:transparent;");
        gl->addWidget(hint, 0, 0, 1, 4);
    }

    const Database::SiteStats t = Database::instance()->totals();
    m_statsLabel->setText(tr("%1 ads and %2 trackers blocked for you so far").arg(t.ads).arg(t.trackers));
}

void StartPage::focusSearch()
{
    if (m_search) m_search->setFocus();
}

void StartPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = ThemeManager::instance()->isDark();
    QColor topCol = dark ? QColor("#1b1c1f") : QColor("#f7f9fa");
    QColor botCol = dark ? QColor("#232428") : QColor("#eef1f3");
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0, topCol);
    grad.setColorAt(1, botCol);
    p.fillRect(rect(), grad);
    QPainterPath wave;
    wave.moveTo(0, height() * 0.86);
    wave.cubicTo(width() * 0.25, height() * 0.8, width() * 0.75, height() * 0.94, width(), height() * 0.88);
    wave.lineTo(width(), height());
    wave.lineTo(0, height());
    wave.closeSubpath();
    QColor ac = ThemeManager::instance()->accent();
    ac.setAlpha(26);
    p.fillPath(wave, ac);
}

void StartPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    update();
}

#include "StartPage.moc"
