#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>

class QLineEdit;
class QVBoxLayout;

// Native start page: search box, speed dial (top sites from real history),
// privacy summary card. All data-driven — no placeholders.
class StartPage : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget *parent = nullptr);
    void refresh();                    // reload speed dial + stats
    void focusSearch();

signals:
    void navigateRequested(const QUrl &url);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void rebuild();
    void buildDial();
    QWidget *tileFor(const QString &title, const QUrl &url, const QString &host);
    QWidget *m_dialHost = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_statsLabel = nullptr;
    QLabel *m_clockLabel = nullptr;
};
