#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QDebug>
#include "mainwindow.h"
#include "settings.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_ANDROID)
    // Qt 6.8 Android: QQuickView in QWidget-App braucht "basic" Render-Loop.
    // Der "threaded" Loop (Qt 6.8 Default) kann in einer QWidget-App keinen
    // EGL-Context erstellen → SIGABRT. Muss vor QApplication gesetzt werden.
    qputenv("QSG_RENDER_LOOP", "basic");
    qputenv("QSG_RHI_BACKEND", "opengl");
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Fahrtenbuch");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("Kloesel");
    app.setOrganizationDomain("fahrtenbuch.local");

    // QSettings: Windows → Registry, Android → SharedPreferences (automatisch)
#if defined(Q_OS_WIN)
    QSettings::setDefaultFormat(QSettings::NativeFormat);  // Windows Registry
#else
    QSettings::setDefaultFormat(QSettings::NativeFormat);  // Android: SharedPreferences
#endif

    qDebug() << "=== Fahrtenbuch Start ===";

    // 1. Einmalige Migration INI → Registry (tut nichts wenn keine INI vorhanden)
    Settings::migrateFromIniIfNeeded();

    // 2. DB-Pfad aus Registry lesen (oder Standardpfad setzen)
    QString dbPath = Settings::getDatabasePath();
    qDebug() << "DB-Pfad:" << dbPath;

    // ── Übersetzung laden ────────────────────────────────────────────────
    // 1. Gespeicherte Spracheinstellung aus Registry/SharedPreferences lesen
    // 2. Falls leer: Systemsprache verwenden
    static QTranslator qtTranslator;   // static: Lebensdauer = App-Laufzeit
    static QTranslator appTranslator;
    {
        QSettings s;
        const QString savedLang = s.value(QStringLiteral("app/language")).toString();
        const QString lang = savedLang.isEmpty()
            ? QLocale::system().name().left(2)   // z.B. "de", "en"
            : savedLang;

        // Qt-eigene Übersetzungen (Dialoge, Buttons)
        if (qtTranslator.load(QStringLiteral("qt_") + lang,
                QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&qtTranslator);

        // App-Übersetzungen aus Ressourcen
        // embed_translations bettet .qm unter :/i18n/ ein
        const QString qmName = QStringLiteral("fahrtenbuch_") + lang;
        bool loaded = appTranslator.load(QStringLiteral(":/i18n/") + qmName)
                   || appTranslator.load(QStringLiteral(":/translations/") + qmName)
                   || appTranslator.load(qmName, QStringLiteral(":/i18n/"))
                   || appTranslator.load(qmName, QStringLiteral(":/translations/"));
        if (loaded) {
            app.installTranslator(&appTranslator);
            qDebug() << "[i18n] Translator geladen:" << qmName;
        } else {
            // Systemsprache nicht unterstützt → Englisch als Fallback
            const QString fallback = QStringLiteral("fahrtenbuch_en");
            bool fbLoaded = appTranslator.load(QStringLiteral(":/i18n/") + fallback)
                         || appTranslator.load(QStringLiteral(":/translations/") + fallback)
                         || appTranslator.load(fallback, QStringLiteral(":/i18n/"))
                         || appTranslator.load(fallback, QStringLiteral(":/translations/"));
            if (fbLoaded) {
                app.installTranslator(&appTranslator);
                qDebug() << "[i18n] Fallback Englisch geladen (Sprache nicht verfügbar:" << lang << ")";
            } else {
                qWarning() << "[i18n] Translator NICHT gefunden:" << qmName
                           << "– UI bleibt auf Quellsprache (Deutsch)";
            }
        }

        qDebug() << "[i18n] savedLang:" << (savedLang.isEmpty() ? "(leer=System)" : savedLang)
                 << "lang:" << lang;
    }

    qDebug() << "MainWindow laden...";
    
    MainWindow w;
    w.show();
    
    qDebug() << "=== Fahrtenbuch bereit ===";  // Registry: HKCU\\SOFTWARE\\Kloesel\\Fahrtenbuch
    
    return app.exec();
}
