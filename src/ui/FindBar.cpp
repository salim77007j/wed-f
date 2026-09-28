#include "FindBar.h"
#include "BrowserTab.h"
#include "ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QToolButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setFixedHeight(40);
    setStyleSheet(QStringLiteral(
        "FindBar { background:%1; border:1px solid %2; }"
        "QLineEdit { background:%3; border:1px solid %4; border-radius:4px; padding:2px 8px; color:%5; }")
        .arg(ThemeManager::instance()->color(RoleToolbar).name(),
             ThemeManager::instance()->color(RoleBorder).name(),
             ThemeManager::instance()->color(RoleInputBg).name(),
             ThemeManager::instance()->color(RoleBorder).name(),
             ThemeManager::instance()->color(RoleText).name()));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 4, 8, 4);
    lay->setSpacing(4);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Find in page"));
    m_edit->setFixedWidth(260);
    lay->addWidget(m_edit);

    m_case = new QToolButton(this);
    m_case->setText("Aa");
    m_case->setCheckable(true);
    m_case->setToolTip(tr("Match case"));
    m_case->setAutoRaise(true);
    connect(m_case, &QToolButton::toggled, this, &FindBar::onCaseToggled);
    lay->addWidget(m_case);

    m_count = new QLabel(this);
    m_count->setStyleSheet("color:#9aa0a6; border:none;");
    lay->addWidget(m_count);

    auto *stretch = new QWidget(this);
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    lay->addWidget(stretch);

    m_prev = new QToolButton(this);
    m_prev->setIcon(Icons::themed("chevron-down"));
    m_prev->setAutoRaise(true);
    m_prev->setToolTip(tr("Previous (Shift+F3)"));
    connect(m_prev, &QToolButton::clicked, this, [this] { findNow(false); });
    lay->addWidget(m_prev);

    m_next = new QToolButton(this);
    m_next->setIcon(Icons::themed("chevron-down"));
    m_next->setAutoRaise(true);
    m_next->setToolTip(tr("Next (F3)"));
    connect(m_next, &QToolButton::clicked, this, [this] { findNow(true); });
    lay->addWidget(m_next);

    m_close = new QToolButton(this);
    m_close->setIcon(Icons::themed("close"));
    m_close->setAutoRaise(true);
    connect(m_close, &QToolButton::clicked, this, [this] {
        hide();
        if (m_tab && m_tab->page())
            m_tab->page()->findText(QString());
    });
    lay->addWidget(m_close);

    connect(m_edit, &QLineEdit::textChanged, this, [this] { findNow(true); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] { findNow(true); });

    auto *f3 = new QShortcut(QKeySequence("F3"), this);
    connect(f3, &QShortcut::activated, this, [this] { findNow(true); });
    auto *sf3 = new QShortcut(QKeySequence("Shift+F3"), this);
    connect(sf3, &QShortcut::activated, this, [this] { findNow(false); });
}

void FindBar::attachTo(BrowserTab *tab)
{
    m_tab = tab;
    reposition();
}

void FindBar::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    reposition();
    m_edit->setFocus();
    m_edit->selectAll();
}

void FindBar::reposition()
{
    if (!parentWidget()) return;
    const int w = qMin(520, parentWidget()->width() - 40);
    setGeometry(parentWidget()->width() - w - 12, 40, w, height());
}

void FindBar::setFocus()
{
    QWidget::setFocus();
    m_edit->setFocus();
    m_edit->selectAll();
}

void FindBar::onCaseToggled() { findNow(true); }

void FindBar::findNow(bool forward)
{
    if (!m_tab || !m_tab->page()) return;
    const QString text = m_edit->text();
    if (text.isEmpty()) {
        m_tab->page()->findText(QString());
        m_count->clear();
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (!forward) flags |= QWebEnginePage::FindBackward;
    if (m_case->isChecked()) flags |= QWebEnginePage::FindCaseSensitively;
    m_tab->page()->findText(text, flags, [this](const QWebEngineFindTextResult &r) {
        if (r.numberOfMatches() > 0)
            m_count->setText(tr("%1/%2").arg(r.activeMatch()).arg(r.numberOfMatches()));
        else
            m_count->setText(tr("No matches"));
    });
}
