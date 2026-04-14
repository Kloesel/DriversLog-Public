#include "settings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#if defined(Q_OS_ANDROID)
#  include <QJniObject>
#endif

static const QString DB_GROUP = "Database";
static const QString DB_KEY   = "DatabasePath";

// ─────────────────────────────────────────────────────────────────────────────
QString Settings::getDefaultDatabasePath()
{
    // Windows  → C:/Users/<user>/AppData/Roaming/.../fahrtenbuch.db
    // Android  → /Android/data/<package>/files/fahrtenbuch.db
    //            Erreichbar per USB (Android-Explorer), kein Sonderpermission nötig.
    //            (getExternalFilesDir – Play-Store-konform, kein MANAGE_EXTERNAL_STORAGE)
#if defined(Q_OS_ANDROID)
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    QString dir;
    if (activity.isValid()) {
        QJniObject jFile = activity.callObjectMethod(
            "getExternalFilesDir",
            "(Ljava/lang/String;)Ljava/io/File;",
            nullptr);
        if (jFile.isValid())
            dir = jFile.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString();
    }
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/fahrtenbuch.db";
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/fahrtenbuch.db";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
QString Settings::getDatabasePath()
{
#if defined(Q_OS_ANDROID)
    // Android: getExternalFilesDir – Play-Store-konform, kein MANAGE_EXTERNAL_STORAGE nötig.
    QString path = getDefaultDatabasePath();
    qDebug() << "Settings (Android): DB-Pfad:" << path;
    return path;
#else
    QSettings s;
    s.beginGroup(DB_GROUP);
    QString path = s.value(DB_KEY).toString();
    s.endGroup();

    if (path.isEmpty()) {
        path = getDefaultDatabasePath();
        setDatabasePath(path);
        qDebug() << "Settings: Kein DB-Pfad — Standardpfad gesetzt:" << path;
    } else {
        // Verzeichnis sicherstellen (kann nach Umzug fehlen)
        QDir().mkpath(QFileInfo(path).absolutePath());
    }
    return path;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void Settings::setDatabasePath(const QString &path)
{
#if defined(Q_OS_ANDROID)
    Q_UNUSED(path)   // Auf Android fix — kein Speichern nötig
#else
    QSettings s;
    s.beginGroup(DB_GROUP);
    s.setValue(DB_KEY, path);
    s.endGroup();
    s.sync();
    qDebug() << "Settings: DB-Pfad in Registry gespeichert:" << path;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void Settings::migrateFromIniIfNeeded()
{
#if defined(Q_OS_ANDROID)
    return;   // Keine INI-Migration auf Android
#else
    QString iniPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    + "/fahrtenbuch.ini";

    if (!QFile::exists(iniPath)) return;

    QSettings s;
    s.beginGroup(DB_GROUP);
    bool alreadyMigrated = s.contains(DB_KEY);
    s.endGroup();
    if (alreadyMigrated) {
        QFile::rename(iniPath, iniPath + ".migrated");
        return;
    }

    QFile file(iniPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString dbPath;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("DatabasePath=")) {
            dbPath = line.mid(13).trimmed();
            break;
        }
    }
    file.close();

    if (!dbPath.isEmpty()) {
        setDatabasePath(dbPath);
        qDebug() << "Settings: DB-Pfad aus INI migriert:" << dbPath;
    }

    QFile::rename(iniPath, iniPath + ".migrated");
    qDebug() << "Settings: INI migriert ->" << (iniPath + ".migrated");
#endif
}
