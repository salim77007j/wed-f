#include "ThemeManager.h"
#include "AppSettings.h"

#include <QApplication>
#include <QFile>
#include <QStyleHints>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

ThemeManager *ThemeManager::instance()
{
    static ThemeManager *s_t = new ThemeManager();
    return s_t;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    m_accent = QColor(AppSettings::instance()->accentColor());
    if (!m_accent.isValid()) m_accent = QColor("#0fa2a0");
    connect(AppSettings::instance(), &AppSettings::themeChanged, this, [this] {
        m_accent = QColor(AppSettings::instance()->accentColor());
        if (!m_accent.isValid()) m_accent = QColor("#0fa2a0");
        apply();
    });
    apply();
}

bool ThemeManager::detectSystemDark() const
{
    if (const QStyleHints *h = QGuiApplication::styleHints())
        return h->colorScheme() == Qt::ColorScheme::Dark;
    return false;
}

QColor ThemeManager::color(ColorRole role) const
{
    const bool d = m_dark;
    switch (role) {
    case RoleText:           return d ? QColor("#e8eaed") : QColor("#202124");
    case RoleTextSecondary:  return d ? QColor("#9aa0a6") : QColor("#5f6368");
    case RoleChrome:         return d ? QColor("#202124") : QColor("#dee1e6");
    case RoleChromeHover:    return d ? QColor("#292a2d") : QColor("#e8eaed");
    case RoleToolbar:        return d ? QColor("#292a2d") : QColor("#ffffff");
    case RoleTab:            return d ? QColor("#1a1b1e") : QColor("#c7cbd1");
    case RoleTabActive:      return d ? QColor("#292a2d") : QColor("#ffffff");
    case RoleBorder:         return d ? QColor("#3c4043") : QColor("#dadce0");
    case RoleAccent:         return m_accent;
    case RoleDanger:         return QColor("#d93025");
    case RoleInputBg:        return d ? QColor("#2f3134") : QColor("#f1f3f4");
    }
    return QColor();
}

void ThemeManager::apply()
{
    int mode = AppSettings::instance()->themeMode();
    m_dark = (mode == Dark) || (mode == System && detectSystemDark());

    QString qss = buildQss();
    qApp->setStyleSheet(qss);
    Icons::refreshAll();
    emit themeApplied(m_dark);
}

QString ThemeManager::buildQss() const
{
    // Template with placeholders substituted for the active palette.
    const QString accentHex = m_accent.name();
    QString base;
    if (m_dark) {
        base = R"(QMainWindow, QDialog { background: #202124; }
QWidget { color: #e8eaed; font-size: 13px; }
QToolBar { background: #292a2d; border: none; }
QMenuBar { background: #202124; }
QMenuBar::item:selected { background: #3c4043; }
QTabBar::tab { background: #1a1b1e; color: #9aa0a6; padding: 5px 12px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }
QTabBar::tab:selected { background: #35363a; color: #e8eaed; }
QTabWidget::pane { border: 1px solid #3c4043; }
QPushButton { background: #35363a; border: 1px solid #3c4043; border-radius: 6px; padding: 6px 14px; color: #e8eaed; }
QPushButton:hover { background: #3c4043; }
QPushButton:pressed { background: #4a4b4f; }
QPushButton:disabled { color: #5f6368; }
QPushButton#accentButton { background: ACCENT; border: none; color: white; font-weight: 600; }
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #2f3134; border: 1px solid #3c4043; border-radius: 6px; padding: 5px 8px; selection-background-color: ACCENT; }
QLineEdit:focus, QComboBox:focus { border-color: ACCENT; }
QComboBox QAbstractItemView { background: #292a2d; border: 1px solid #3c4043; selection-background-color: ACCENT; }
QListWidget, QTreeView, QTableView, QTreeView, QTableWidget { background: #26272b; alternate-background-color: #2a2b2f; border: 1px solid #3c4043; border-radius: 6px; selection-background-color: ACCENT; selection-color: white; }
QHeaderView::section { background: #292a2d; border: none; border-bottom: 1px solid #3c4043; padding: 5px; }
QStatusBar { background: #202124; color: #9aa0a6; }
QDockWidget { titlebar-close-icon: none; color: #e8eaed; }
QToolTip { background: #35363a; color: #e8eaed; border: 1px solid #3c4043; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #5f6368; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #9aa0a6; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #5f6368; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #9aa0a6; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
QCheckBox::indicator:unchecked, QRadioButton::indicator:unchecked { border: 1px solid #5f6368; border-radius: 3px; background: #2f3134; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { border: 1px solid ACCENT; border-radius: 3px; background: ACCENT; }
QGroupBox { border: 1px solid #3c4043; border-radius: 6px; margin-top: 10px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #9aa0a6; }
QFrame[frameShape="4"] { color: #3c4043; } )";
    } else {
        base = R"(QMainWindow, QDialog { background: #ffffff; }
QWidget { color: #202124; font-size: 13px; }
QToolBar { background: #dee1e6; border: none; }
QMenuBar { background: #ffffff; }
QMenuBar::item:selected { background: #e8eaed; }
QTabBar::tab { background: #c7cbd1; color: #5f6368; padding: 5px 12px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }
QTabBar::tab:selected { background: #ffffff; color: #202124; }
QTabWidget::pane { border: 1px solid #dadce0; }
QPushButton { background: #ffffff; border: 1px solid #dadce0; border-radius: 6px; padding: 6px 14px; color: #202124; }
QPushButton:hover { background: #f1f3f4; }
QPushButton:pressed { background: #e8eaed; }
QPushButton:disabled { color: #9aa0a6; }
QPushButton#accentButton { background: ACCENT; border: none; color: white; font-weight: 600; }
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #ffffff; border: 1px solid #dadce0; border-radius: 6px; padding: 5px 8px; selection-background-color: ACCENT; }
QLineEdit:focus, QComboBox:focus { border-color: ACCENT; }
QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #dadce0; selection-background-color: #e8eaed; }
QListWidget, QTreeView, QTableView, QTableWidget { background: #ffffff; alternate-background-color: #f8f9fa; border: 1px solid #dadce0; border-radius: 6px; selection-background-color: ACCENT; selection-color: white; }
QHeaderView::section { background: #f8f9fa; border: none; border-bottom: 1px solid #dadce0; padding: 5px; }
QStatusBar { background: #f1f3f4; color: #5f6368; }
QDockWidget { color: #202124; }
QToolTip { background: #ffffff; color: #202124; border: 1px solid #dadce0; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #c7cbd1; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #9aa0a6; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #c7cbd1; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #9aa0a6; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
QCheckBox::indicator:unchecked, QRadioButton::indicator:unchecked { border: 1px solid #9aa0a6; border-radius: 3px; background: #ffffff; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { border: 1px solid ACCENT; border-radius: 3px; background: ACCENT; }
QGroupBox { border: 1px solid #dadce0; border-radius: 6px; margin-top: 10px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #5f6368; }
QFrame[frameShape="4"] { color: #dadce0; } )";
    }
    return base.replace("ACCENT", accentHex);
}

// ---------------------------------------------------------------- icon engine

namespace {
QMutex s_iconMutex;
QHash<QString, QIcon> s_iconCache;

void strokeIcon(QPainter &p, const QPolygonF &pts, qreal width = 1.8)
{
    p.save();
    QPen pen(p.pen());
    pen.setWidthF(width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    QPainterPath path;
    path.moveTo(pts.first());
    for (int i = 1; i < pts.size(); ++i) path.lineTo(pts.at(i));
    p.drawPath(path);
    p.restore();
}

QPixmap drawIcon(const QString &name, const QColor &color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal s = qreal(size);
    auto X = [s](qreal x) { return x * s; };
    auto Y = [s](qreal y) { return y * s; };
    auto P = [&](qreal x, qreal y) { return QPointF(X(x), Y(y)); };

    if (name == "back") {
        strokeIcon(p, {P(0.62, 0.2), P(0.3, 0.5), P(0.62, 0.8)});
        p.drawLine(QPointF(X(0.3), Y(0.5)), QPointF(X(0.75), Y(0.5)));
    } else if (name == "forward") {
        strokeIcon(p, {P(0.38, 0.2), P(0.7, 0.5), P(0.38, 0.8)});
        p.drawLine(QPointF(X(0.25), Y(0.5)), QPointF(X(0.7), Y(0.5)));
    } else if (name == "reload") {
        p.drawArc(QRectF(X(0.22), Y(0.22), X(0.56), Y(0.56)), 30 * 16, 280 * 16);
        strokeIcon(p, {P(0.5, 0.14), P(0.68, 0.26), P(0.5, 0.36)});
    } else if (name == "stop") {
        strokeIcon(p, {P(0.28, 0.28), P(0.72, 0.72)});
        strokeIcon(p, {P(0.72, 0.28), P(0.28, 0.72)});
    } else if (name == "home") {
        strokeIcon(p, {P(0.15, 0.5), P(0.5, 0.18), P(0.85, 0.5)});
        p.drawLine(QPointF(X(0.28), Y(0.52)), QPointF(X(0.28), Y(0.84)));
        p.drawLine(QPointF(X(0.72), Y(0.52)), QPointF(X(0.72), Y(0.84)));
        p.drawLine(QPointF(X(0.28), Y(0.84)), QPointF(X(0.72), Y(0.84)));
    } else if (name == "star" || name == "star-filled") {
        QPolygonF star;
        const qreal cx = 0.5, cy = 0.52, R = 0.34, r = 0.14;
        for (int i = 0; i < 10; ++i) {
            qreal ang = -M_PI / 2 + i * M_PI / 5;
            qreal rad = (i % 2 == 0) ? R : r;
            star << QPointF(X(cx + rad * qCos(ang)), Y(cy + rad * qSin(ang)));
        }
        if (name == "star-filled") {
            p.setBrush(color);
            p.drawPolygon(star);
        } else {
            p.drawPolygon(star);
        }
    } else if (name == "shield" || name == "shield-off") {
        QPolygonF shield;
        shield << P(0.5, 0.12) << P(0.86, 0.26) << P(0.86, 0.5)
               << P(0.5, 0.88) << P(0.14, 0.5) << P(0.14, 0.26);
        p.drawPolygon(shield);
        if (name == "shield-off") {
            strokeIcon(p, {P(0.2, 0.9), P(0.8, 0.1)});
        } else {
            strokeIcon(p, {P(0.35, 0.5), P(0.46, 0.62), P(0.67, 0.38)}, 1.6);
        }
    } else if (name == "lock") {
        p.drawRoundedRect(QRectF(X(0.28), Y(0.46), X(0.44), Y(0.4)), 3, 3);
        QPainterPath arc;
        arc.arcMoveTo(QRectF(X(0.35), Y(0.2), X(0.3), Y(0.34)), 0);
        arc.arcTo(QRectF(X(0.35), Y(0.2), X(0.3), Y(0.34)), 180, -180);
        QPen pen2(color); pen2.setWidthF(1.8); p.setPen(pen2);
        p.drawPath(arc);
    } else if (name == "info") {
        p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.36), Y(0.36));
        p.drawPoint(QPointF(X(0.5), Y(0.32)));
        p.drawLine(QPointF(X(0.5), Y(0.45)), QPointF(X(0.5), Y(0.68)));
    } else if (name == "warn") {
        strokeIcon(p, {P(0.5, 0.15), P(0.88, 0.82), P(0.12, 0.82), P(0.5, 0.15)});
        p.drawPoint(QPointF(X(0.5), Y(0.45)));
        p.drawLine(QPointF(X(0.5), Y(0.58)), QPointF(X(0.5), Y(0.72)));
    } else if (name == "search") {
        p.drawEllipse(QPointF(X(0.44), Y(0.42)), X(0.22), Y(0.22));
        p.drawLine(QPointF(X(0.6), Y(0.58)), QPointF(X(0.82), Y(0.8)));
    } else if (name == "download") {
        strokeIcon(p, {P(0.5, 0.14), P(0.5, 0.66)});
        strokeIcon(p, {P(0.32, 0.5), P(0.5, 0.68), P(0.68, 0.5)});
        p.drawLine(QPointF(X(0.2), Y(0.85)), QPointF(X(0.8), Y(0.85)));
    } else if (name == "menu") {
        for (int i = 0; i < 3; ++i)
            p.drawPoint(QPointF(X(0.5), Y(0.3 + i * 0.2)));
        QPen pen2(color); pen2.setWidthF(4.5); pen2.setCapStyle(Qt::RoundCap);
        p.setPen(pen2);
        for (int i = 0; i < 3; ++i)
            p.drawPoint(QPointF(X(0.5), Y(0.3 + i * 0.2)));
    } else if (name == "plus") {
        p.drawLine(QPointF(X(0.5), Y(0.22)), QPointF(X(0.5), Y(0.78)));
        p.drawLine(QPointF(X(0.22), Y(0.5)), QPointF(X(0.78), Y(0.5)));
    } else if (name == "close") {
        strokeIcon(p, {P(0.28, 0.28), P(0.72, 0.72)});
        strokeIcon(p, {P(0.72, 0.28), P(0.28, 0.72)});
    } else if (name == "restore") {
        strokeIcon(p, {P(0.7, 0.28), P(0.32, 0.66), P(0.66, 0.66)});
        p.drawLine(QPointF(X(0.32), Y(0.3)), QPointF(X(0.32), Y(0.66)));
    } else if (name == "mute") {
        p.drawPolygon({P(0.3, 0.4), P(0.42, 0.4), P(0.58, 0.24), P(0.58, 0.76),
                       P(0.42, 0.6), P(0.3, 0.6)});
        strokeIcon(p, {P(0.66, 0.42), P(0.82, 0.58)});
        strokeIcon(p, {P(0.82, 0.42), P(0.66, 0.58)});
    } else if (name == "tab-audio") {
        p.drawPolygon({P(0.3, 0.4), P(0.42, 0.4), P(0.58, 0.24), P(0.58, 0.76),
                       P(0.42, 0.6), P(0.3, 0.6)});
        strokeIcon(p, {P(0.68, 0.36), P(0.8, 0.5), P(0.92, 0.64)});
        strokeIcon(p, {P(0.92, 0.36), P(0.8, 0.5), P(0.68, 0.64)});
    } else if (name == "private") {
        p.drawEllipse(QPointF(X(0.5), Y(0.55)), X(0.34), Y(0.3));
        p.drawEllipse(QPointF(X(0.42), Y(0.5)), X(0.05), Y(0.05));
        p.drawEllipse(QPointF(X(0.58), Y(0.5)), X(0.05), Y(0.05));
    } else if (name == "settings") {
        QPen pen2(color); pen2.setWidthF(1.6); p.setPen(pen2);
        for (int i = 0; i < 6; ++i) {
            qreal a = i * M_PI / 3;
            p.drawLine(QPointF(X(0.5 + 0.22 * qCos(a)), Y(0.5 + 0.22 * qSin(a))),
                       QPointF(X(0.5 + 0.4 * qCos(a)), Y(0.5 + 0.4 * qSin(a))));
        }
        p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.14), Y(0.14));
    } else if (name == "history") {
        p.drawArc(QRectF(X(0.2), Y(0.2), X(0.6), Y(0.6)), 20 * 16, 320 * 16);
        strokeIcon(p, {P(0.5, 0.5), P(0.5, 0.3)});
        strokeIcon(p, {P(0.5, 0.5), P(0.66, 0.58)});
    } else if (name == "bookmark") {
        strokeIcon(p, {P(0.3, 0.14), P(0.3, 0.86), P(0.5, 0.66), P(0.7, 0.86), P(0.7, 0.14)});
    } else if (name == "devtools") {
        strokeIcon(p, {P(0.22, 0.2), P(0.5, 0.5), P(0.22, 0.8)});
        p.drawLine(QPointF(X(0.45), Y(0.8)), QPointF(X(0.8), Y(0.8)));
    } else if (name == "find") {
        p.drawEllipse(QPointF(X(0.44), Y(0.42)), X(0.22), Y(0.22));
        p.drawLine(QPointF(X(0.6), Y(0.58)), QPointF(X(0.82), Y(0.8)));
        p.drawLine(QPointF(X(0.35), Y(0.42)), QPointF(X(0.5), Y(0.42)));
        p.drawLine(QPointF(X(0.35), Y(0.48)), QPointF(X(0.55), Y(0.48)));
    } else if (name == "print") {
        p.drawRect(QRectF(X(0.26), Y(0.4), X(0.48), Y(0.28)));
        p.drawRect(QRectF(X(0.36), Y(0.14), X(0.28), Y(0.26)));
        p.drawRect(QRectF(X(0.36), Y(0.62), X(0.28), Y(0.24)));
    } else if (name == "zoomin") {
        p.drawEllipse(QPointF(X(0.44), Y(0.42)), X(0.22), Y(0.22));
        p.drawLine(QPointF(X(0.6), Y(0.58)), QPointF(X(0.82), Y(0.8)));
        p.drawLine(QPointF(X(0.36), Y(0.42)), QPointF(X(0.52), Y(0.42)));
        p.drawLine(QPointF(X(0.44), Y(0.34)), QPointF(X(0.44), Y(0.5)));
    } else if (name == "zoomout") {
        p.drawEllipse(QPointF(X(0.44), Y(0.42)), X(0.22), Y(0.22));
        p.drawLine(QPointF(X(0.6), Y(0.58)), QPointF(X(0.82), Y(0.8)));
        p.drawLine(QPointF(X(0.36), Y(0.42)), QPointF(X(0.52), Y(0.42)));
    } else if (name == "zoomreset") {
        p.drawEllipse(QPointF(X(0.44), Y(0.42)), X(0.22), Y(0.22));
        p.drawLine(QPointF(X(0.6), Y(0.58)), QPointF(X(0.82), Y(0.8)));
        p.drawText(QRectF(X(0.35), Y(0.37), X(0.18), Y(0.1)), Qt::AlignCenter, "0");
    } else if (name == "globe") {
        p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.36), Y(0.36));
        p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.16), Y(0.36));
        p.drawLine(QPointF(X(0.14), Y(0.5)), QPointF(X(0.86), Y(0.5)));
    } else if (name == "copy") {
        p.drawRect(QRectF(X(0.3), Y(0.3), X(0.5), Y(0.56)));
        strokeIcon(p, {P(0.42, 0.14), P(0.76, 0.14), P(0.76, 0.62), P(0.7, 0.62)});
    } else if (name == "folder") {
        QPainterPath folderPath;
        folderPath.moveTo(X(0.14), Y(0.36));
        folderPath.lineTo(X(0.14), Y(0.84));
        folderPath.lineTo(X(0.86), Y(0.84));
        folderPath.lineTo(X(0.86), Y(0.28));
        folderPath.lineTo(X(0.46), Y(0.28));
        folderPath.lineTo(X(0.38), Y(0.18));
        folderPath.lineTo(X(0.14), Y(0.18));
        folderPath.closeSubpath();
        p.drawPath(folderPath);
    } else if (name == "external") {
        strokeIcon(p, {P(0.4, 0.6), P(0.78, 0.22)});
        strokeIcon(p, {P(0.5, 0.22), P(0.78, 0.22), P(0.78, 0.5)});
        strokeIcon(p, {P(0.6, 0.6), P(0.24, 0.6), P(0.24, 0.84), P(0.6, 0.84)});
    } else if (name == "check") {
        strokeIcon(p, {P(0.2, 0.55), P(0.42, 0.75), P(0.8, 0.3)});
    } else if (name == "chevron-down") {
        strokeIcon(p, {P(0.3, 0.38), P(0.5, 0.6), P(0.7, 0.38)});
    } else if (name == "chevron-right") {
        strokeIcon(p, {P(0.4, 0.3), P(0.62, 0.5), P(0.4, 0.7)});
    } else if (name == "trash") {
        p.drawLine(QPointF(X(0.24), Y(0.3)), QPointF(X(0.76), Y(0.3)));
        p.drawRect(QRectF(X(0.3), Y(0.3), X(0.4), Y(0.54)));
        strokeIcon(p, {P(0.42, 0.3), P(0.42, 0.2), P(0.58, 0.2), P(0.58, 0.3)});
    } else if (name == "edit") {
        strokeIcon(p, {P(0.68, 0.16), P(0.84, 0.32), P(0.38, 0.78), P(0.2, 0.8), P(0.22, 0.62)});
    } else if (name == "window") {
        p.drawRect(QRectF(X(0.14), Y(0.18), X(0.72), Y(0.64)));
        p.drawLine(QPointF(X(0.14), Y(0.36)), QPointF(X(0.86), Y(0.36)));
    } else if (name == "camera" || name == "mic" || name == "location" || name == "notification"
               || name == "clipboard" || name == "mouse") {
        // permission icons: simple distinct shapes
        if (name == "camera") {
            p.drawRoundedRect(QRectF(X(0.18), Y(0.32), X(0.5), Y(0.36)), 3, 3);
            strokeIcon(p, {P(0.68, 0.42), P(0.82, 0.3), P(0.82, 0.7), P(0.68, 0.58)});
            p.drawEllipse(QPointF(X(0.43), Y(0.5)), X(0.1), Y(0.1));
        } else if (name == "mic") {
            p.drawRoundedRect(QRectF(X(0.4), Y(0.16), X(0.2), Y(0.42)), 6, 6);
            strokeIcon(p, {P(0.32, 0.5), P(0.32, 0.58), P(0.5, 0.76), P(0.68, 0.58), P(0.68, 0.5)});
            p.drawLine(QPointF(X(0.5), Y(0.76)), QPointF(X(0.5), Y(0.86)));
        } else if (name == "location") {
            QPainterPath pin;
            pin.moveTo(X(0.5), Y(0.88));
            pin.quadTo(X(0.18), Y(0.5), X(0.34), Y(0.3));
            pin.quadTo(X(0.5), Y(0.14), X(0.66), Y(0.3));
            pin.quadTo(X(0.82), Y(0.5), X(0.5), Y(0.88));
            p.drawPath(pin);
            p.drawEllipse(QPointF(X(0.5), Y(0.38)), X(0.08), Y(0.08));
        } else if (name == "notification") {
            p.drawArc(QRectF(X(0.3), Y(0.28), X(0.4), Y(0.4)), 180 * 16, -180 * 16);
            p.drawRect(QRectF(X(0.3), Y(0.48), X(0.4), Y(0.24)));
            strokeIcon(p, {P(0.44, 0.76), P(0.56, 0.76)});
            p.drawEllipse(QPointF(X(0.5), Y(0.84)), X(0.04), Y(0.04));
        } else if (name == "clipboard") {
            p.drawRoundedRect(QRectF(X(0.3), Y(0.22), X(0.4), Y(0.6)), 3, 3);
            p.drawRect(QRectF(X(0.42), Y(0.16), X(0.16), Y(0.12)));
        } else if (name == "mouse") {
            p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.2), Y(0.32));
            p.drawLine(QPointF(X(0.5), Y(0.2)), QPointF(X(0.5), Y(0.44)));
            p.drawLine(QPointF(X(0.3), Y(0.44)), QPointF(X(0.7), Y(0.44)));
        }
    } else {
        // fallback: circle
        p.drawEllipse(QPointF(X(0.5), Y(0.5)), X(0.3), Y(0.3));
    }
    p.end();
    return pm;
}
} // namespace

namespace Icons {

QPixmap pixmap(const QString &name, const QColor &color, int size)
{
    return drawIcon(name, color, size);
}

QIcon make(const QString &name, const QColor &color, int size)
{
    QIcon icon;
    for (int s : {16, 20, 24, 32})
        icon.addPixmap(drawIcon(name, color, s));
    return icon;
}

QIcon themed(const QString &name, int size)
{
    QMutexLocker lock(&s_iconMutex);
    const QString key = ThemeManager::instance()->isDark() ? "d/" + name : "l/" + name;
    auto it = s_iconCache.find(key);
    if (it != s_iconCache.end()) return *it;
    const QColor c = ThemeManager::instance()->color(RoleTextSecondary);
    QIcon icon = make(name, c, size);
    s_iconCache.insert(key, icon);
    return icon;
}

void refreshAll()
{
    QMutexLocker lock(&s_iconMutex);
    s_iconCache.clear();
}

} // namespace Icons
