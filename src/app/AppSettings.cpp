#include "AppSettings.h"

#include <QDir>
#include <QSet>
#include <QStandardPaths>
#include <QCoreApplication>

AppSettings *AppSettings::instance()
{
    static AppSettings s_self;
    return &s_self;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent),
      m_s(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
              + "/wed/settings.ini", QSettings::IniFormat)
{
    QDir().mkpath(QFileInfo(m_s.fileName()).absolutePath());
}

QString AppSettings::downloadDir() const
{
    QString d = m_s.value("general/downloadDir").toString();
    if (d.isEmpty()) {
        d = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (d.isEmpty())
            d = QDir::homePath() + "/Downloads";
    }
    return d;
}

QString AppSettings::fullVersionString()
{
    return QStringLiteral("WED Browser %1 (Chromium %2, Qt %3)")
        .arg(versionString())
        .arg(QString::fromLatin1(qVersion()))
        .arg(QString::fromLatin1(qVersion()));
}

QList<AppSettings::SearchEngineInfo> AppSettings::searchEngines()
{
    return {
        {"duckduckgo", "DuckDuckGo",
         "https://duckduckgo.com/?q=%1", "https://duckduckgo.com/ac/?q=%1&type=list"},
        {"google", "Google",
         "https://www.google.com/search?q=%1", "https://www.google.com/complete/search?client=firefox&q=%1"},
        {"bing", "Bing",
         "https://www.bing.com/search?q=%1", "https://www.bing.com/osjson.aspx?query=%1"},
        {"startpage", "Startpage",
         "https://www.startpage.com/sp/search?query=%1", ""},
        {"wikipedia", "Wikipedia",
         "https://en.wikipedia.org/wiki/Special:Search?search=%1", ""},
        {"ecosia", "Ecosia",
         "https://www.ecosia.org/search?q=%1", ""},
    };
}

// ---------------------------------------------------------------- utilities

QString Utils::hostOfUrl(const QUrl &url)
{
    QString h = url.host();
    if (h.isEmpty()) {
        // fallback for odd urls
        QString s = url.toString();
        int i = s.indexOf("://");
        if (i >= 0) {
            s = s.mid(i + 3);
            h = s.left(s.indexOf('/'));
        }
    }
    if (h.isEmpty()) return QString();
    h = h.toLower();
    if (h.startsWith("www.")) h = h.mid(4);
    return h;
}

QString Utils::registrableDomain(const QString &host)
{
    if (host.isEmpty()) return QString();
    static const QSet<QString> twoPartSuffixes = {
        "co.uk", "org.uk", "ac.uk", "gov.uk", "co.jp", "or.jp", "ne.jp", "co.kr",
        "co.za", "com.au", "net.au", "org.au", "com.br", "com.cn", "com.tw",
        "com.hk", "com.sg", "com.my", "com.ar", "com.mx", "co.in", "co.id",
        "com.tr", "com.vn", "com.ph", "com.ng", "co.ke", "com.eg", "co.il",
        "com.pk", "com.bd", "co.nz"};
    const QStringList parts = host.split('.');
    if (parts.size() >= 3) {
        const QString lastTwo = parts.at(parts.size() - 2) + "." + parts.last();
        if (twoPartSuffixes.contains(lastTwo))
            return parts.mid(parts.size() - 3).join('.');
    }
    if (parts.size() >= 2)
        return parts.mid(parts.size() - 2).join('.');
    return host;
}

QString Utils::formatBytes(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
}
