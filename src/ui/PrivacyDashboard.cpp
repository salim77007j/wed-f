#include "PrivacyDashboard.h"

#include "AppSettings.h"
#include "Database.h"
#include "PrivacyEngine.h"
#include "ThemeManager.h"
#include "Utils.h"

#include <QTableWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFrame>
#include <QTableWidgetItem>

PrivacyDashboard::PrivacyDashboard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Privacy dashboard"));
    setModal(true);
    resize(720, 560);
    buildUi();
    refreshData();
}

QWidget *PrivacyDashboard::buildStatCard(const QString &title, const QString &value, const QString &sub) const
{
    auto *card = new QFrame;
    card->setProperty("card", true);
    card->setFrameShape(QFrame::StyledPanel);
    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(16, 12, 16, 12);
    auto *t = new QLabel(title, card);
    t->setObjectName("cardTitle");
    auto *v = new QLabel(value, card);
    v->setObjectName("cardValue");
    auto *s = new QLabel(sub, card);
    s->setObjectName("cardSub");
    s->setWordWrap(true);
    l->addWidget(t);
    l->addWidget(v);
    l->addWidget(s);
    return card;
}

void PrivacyDashboard::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *head = new QLabel(tr("<span style='font-size:16pt;font-weight:700;'>Privacy dashboard</span><br>"
                               "<span style='color:%2'>What WED has shielded you from</span>")
                             .arg(ThemeManager::instance()->color(RoleTextSecondary).name()), this);
    head->setTextFormat(Qt::RichText);
    root->addWidget(head);

    // stat cards row
    auto *cards = new QGridLayout;
    cards->setSpacing(10);
    m_sessionAds = new QLabel;
    m_sessionTrackers = new QLabel;
    m_alltimeAds = new QLabel;
    m_alltimeTrackers = new QLabel;

    auto *c1 = buildStatCard(tr("Ads blocked this session"), QString::number(0), tr("Since WED started"));
    auto *c2 = buildStatCard(tr("Trackers blocked this session"), QString::number(0), tr("Since WED started"));
    auto *c3 = buildStatCard(tr("Ads blocked all-time"), QString::number(0), tr("Stored in local database"));
    auto *c4 = buildStatCard(tr("Trackers blocked all-time"), QString::number(0), tr("Stored in local database"));
    cards->addWidget(c1, 0, 0);
    cards->addWidget(c2, 0, 1);
    cards->addWidget(c3, 1, 0);
    cards->addWidget(c4, 1, 1);
    root->addLayout(cards);

    // protections overview
    auto *protLbl = new QLabel(tr("<b>Protections status</b>"), this);
    root->addWidget(protLbl);
    m_rules = new QLabel(this);
    m_rules->setWordWrap(true);
    root->addWidget(m_rules);
    m_lists = new QLabel(this);
    m_lists->setWordWrap(true);
    root->addWidget(m_lists);

    // per-site table
    auto *sitesLbl = new QLabel(tr("<b>Top sites by blocked items</b>"), this);
    root->addWidget(sitesLbl);
    m_sites = new QTableWidget(this);
    m_sites->setColumnCount(3);
    m_sites->setHorizontalHeaderLabels({ tr("Site"), tr("Ads"), tr("Trackers") });
    m_sites->verticalHeader()->setVisible(false);
    m_sites->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_sites->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sites->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sites->setAlternatingRowColors(true);
    root->addWidget(m_sites, 2);

    // exceptions
    auto *exLbl = new QLabel(tr("<b>Sites with protections turned off</b>"), this);
    root->addWidget(exLbl);
    auto *exRow = new QHBoxLayout;
    m_exceptions = new QListWidget(this);
    m_exceptions->setMaximumHeight(110);
    exRow->addWidget(m_exceptions, 1);
    auto *rm = new QPushButton(tr("Re-enable"), this);
    connect(rm, &QPushButton::clicked, this, &PrivacyDashboard::onRemoveException);
    exRow->addWidget(rm, 0, Qt::AlignTop);
    root->addLayout(exRow);

    auto *btns = new QHBoxLayout;
    btns->addStretch(1);
    auto *close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    btns->addWidget(close);
    root->addLayout(btns);
}

void PrivacyDashboard::refreshData()
{
    PrivacyEngine *pe = PrivacyEngine::instance();
    Database *db = Database::instance();

    const Database::SiteStats all = db->totals();
    const quint64 sAds = pe->sessionAds();
    const quint64 sTr = pe->sessionTrackers();

    // refresh card value labels (find the value labels inside cards)
    const QList<QLabel *> allLabels = findChildren<QLabel *>();
    QVector<QLabel *> vals;
    for (QLabel *l : allLabels) {
        if (l->objectName() == QLatin1String("cardValue"))
            vals.append(l);
    }
    if (vals.size() >= 4) {
        vals[0]->setText(QString::number(sAds));
        vals[1]->setText(QString::number(sTr));
        vals[2]->setText(QString::number(all.ads));
        vals[3]->setText(QString::number(all.trackers));
    }

    AppSettings *s = AppSettings::instance();
    QStringList prot;
    prot << tr("Ad blocking: %1").arg(s->blockAds() ? tr("ON") : tr("off"))
         << tr("Tracker blocking: %1").arg(s->blockTrackers() ? tr("ON") : tr("off"))
         << tr("Cookie policy: %1").arg(s->cookiePolicy() == 0 ? tr("allow all")
                                      : s->cookiePolicy() == 1 ? tr("block third-party")
                                                               : tr("block all"))
         << tr("Fingerprint protection: %1").arg(s->fingerprintProtection() ? tr("ON") : tr("off"))
         << tr("HTTPS-first: %1").arg(s->httpsFirst() ? tr("ON") : tr("off"))
         << tr("Do-Not-Track / GPC headers: %1 / %2")
                .arg(s->dntHeader() ? tr("on") : tr("off"))
                .arg(s->gpcHeader() ? tr("on") : tr("off"));
    m_rules->setText(prot.join(QStringLiteral(" &nbsp;•&nbsp; ")));

    const bool el = s->easyListEnabled(), ep = s->easyPrivacyEnabled();
    m_lists->setText(tr("Filter lists: EasyList (%1), EasyPrivacy (%2) — %L3 active rules")
                     .arg(el ? tr("enabled") : tr("disabled"),
                          ep ? tr("enabled") : tr("disabled"),
                          pe->ruleCount()));

    const QList<Database::SiteStats> rows = db->siteStats(50);
    m_sites->setRowCount(rows.size());
    int r = 0;
    for (const Database::SiteStats &st : rows) {
        auto *i0 = new QTableWidgetItem(st.host);
        auto *i1 = new QTableWidgetItem;
        i1->setData(Qt::DisplayRole, st.ads);
        auto *i2 = new QTableWidgetItem;
        i2->setData(Qt::DisplayRole, st.trackers);
        if (r % 2 == 1)
            for (QTableWidgetItem *i : { i0, i1, i2 })
                i->setData(Qt::BackgroundRole, QBrush(ThemeManager::instance()->color(RoleChrome)));
        m_sites->setItem(r, 0, i0);
        m_sites->setItem(r, 1, i1);
        m_sites->setItem(r, 2, i2);
        ++r;
    }

    m_exceptions->clear();
    m_exceptions->addItems(db->shieldExceptions());
}

void PrivacyDashboard::onRemoveException()
{
    const int row = m_exceptions->currentRow();
    if (row < 0)
        return;
    const QString host = m_exceptions->item(row)->text();
    Database::instance()->setShieldException(host, false);
    PrivacyEngine::instance()->refreshExceptions();
    refreshData();
}
