#pragma once
#include <QWidget>

class BrowserTab;
class QLineEdit;
class QLabel;
class QToolButton;

// In-page find bar: real QWebEnginePage::findText with match counting,
// case sensitivity, prev/next. Overlay positioned at top of the window.
class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    void attachTo(BrowserTab *tab);
    void setFocus();

protected:
    void showEvent(QShowEvent *e) override;

private slots:
    void findNow(bool forward = true);
    void onCaseToggled();

private:
    void reposition();

    QLineEdit *m_edit = nullptr;
    QToolButton *m_case = nullptr;
    QToolButton *m_prev = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_close = nullptr;
    QLabel *m_count = nullptr;
    BrowserTab *m_tab = nullptr;
};
