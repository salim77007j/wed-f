#pragma once
#include <QObject>
#include <QIcon>
#include <QColor>

// Theme management: light / dark / follow-system, live QSS re-load, accent color.
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Mode { System = 0, Light = 1, Dark = 2 };

    static ThemeManager *instance();

    void apply();                       // re-apply current theme to the app
    bool isDark() const { return m_dark; }
    QColor accent() const { return m_accent; }
    QColor color(ColorRole role) const;

signals:
    void themeApplied(bool dark);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    bool detectSystemDark() const;
    QString buildQss() const;
    bool m_dark = false;
    QColor m_accent;
};

// Roles used across custom widgets
enum ColorRole {
    RoleText,
    RoleTextSecondary,
    RoleChrome,
    RoleChromeHover,
    RoleToolbar,
    RoleTab,
    RoleTabActive,
    RoleBorder,
    RoleAccent,
    RoleDanger,
    RoleInputBg,
};

// Runtime-drawn flat icon set — recolored per theme, no external files.
namespace Icons {
QIcon make(const QString &name, const QColor &color, int size = 20);
// convenience: current theme colors
QIcon themed(const QString &name, int size = 20);
QPixmap pixmap(const QString &name, const QColor &color, int size);
void refreshAll();  // clear cache (theme change)
}
