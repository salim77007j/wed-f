#pragma once
#include <QDialog>

class QTableWidget;
class QListWidget;
class QLabel;

// Global privacy dashboard: session + all-time block counters, per-site stats
// from the database, loaded filter rules, protection toggles summary, and
// shield exceptions management. All data comes from PrivacyEngine / Database.
class PrivacyDashboard : public QDialog
{
    Q_OBJECT
public:
    explicit PrivacyDashboard(QWidget *parent = nullptr);

private slots:
    void refreshData();
    void onRemoveException();

private:
    QWidget *buildStatCard(const QString &title, const QString &value, const QString &sub) const;
    void buildUi();

    QLabel *m_sessionAds = nullptr;
    QLabel *m_sessionTrackers = nullptr;
    QLabel *m_alltimeAds = nullptr;
    QLabel *m_alltimeTrackers = nullptr;
    QLabel *m_rules = nullptr;
    QLabel *m_lists = nullptr;
    QTableWidget *m_sites = nullptr;
    QListWidget *m_exceptions = nullptr;
};
