QT += core gui widgets sql network printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Release-Build Optimierungen
CONFIG(release, debug|release) {
    CONFIG   += optimize_full
    DEFINES  += QT_NO_DEBUG_OUTPUT
}

TARGET   = Fahrtenbuch
TEMPLATE = app

# Versionsänderung: nur diese eine Zeile anpassen
# Build-Suffix = aktuelle Uhrzeit (HHmm) – nur im Debug-Build.
# Release-Build hat keinen Zeitstempel (saubere Versionsnummer für Store/Installer).
CONFIG(debug, debug|release) {
    android|win32 {
        # Android wird auf Windows cross-kompiliert → powershell für Uhrzeit
        BUILD_TIME = $$system(powershell -Command "Get-Date -Format 'HHmm'")
    } else {
        BUILD_TIME = $$system(date +%H%M)
    }
    VERSION = 1.4.27.$$BUILD_TIME
} else {
    VERSION = 1.4.27
}
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# Übersetzungen – mit "lupdate Fahrtenbuch.pro" Quelltext scannen,
# mit "lrelease Fahrtenbuch.pro" .qm-Dateien generieren.
TRANSLATIONS = translations/fahrtenbuch_de.ts \
               translations/fahrtenbuch_en.ts \
               translations/fahrtenbuch_fr.ts \
               translations/fahrtenbuch_nl.ts \
               translations/fahrtenbuch_es.ts
# lrelease + embed_translations für beide Build-Konfigurationen aktivieren.
# Ohne "CONFIG(debug, ...)" läuft lrelease nur im Release-Build.
CONFIG += lrelease embed_translations
CONFIG(debug, debug|release) {
    # Im Debug-Build .qm explizit neu generieren (qmake-Variable erzwingen)
    QMAKE_LRELEASE_FLAGS += -nounfinished
}

INCLUDEPATH += include

HEADERS += \
    include/aboutdialog.h \
    include/adresstable.h \
    include/database.h \
    include/databasesync.h \
    include/datenexport.h \
    include/devicediscovery.h \
    include/distanceservice.h \
    include/drawerwidget.h \
    include/einstellungen.h \
    include/fahrertable.h \
    include/fahrtentable.h \
    include/filetransfer.h \
    include/helpwindow.h \
    include/mainwindow.h \
    include/models.h \
    include/settings.h \
    include/snackbar.h \
    include/swipefilter.h \
    include/synclogview.h \
    include/syncmanager.h \
    include/version.h

SOURCES += \
    src/aboutdialog.cpp \
    src/adresstable.cpp \
    src/database.cpp \
    src/databasesync.cpp \
    src/datenexport.cpp \
    src/devicediscovery.cpp \
    src/distanceservice.cpp \
    src/drawerwidget.cpp \
    src/einstellungen.cpp \
    src/fahrertable.cpp \
    src/fahrtentable.cpp \
    src/filetransfer.cpp \
    src/helpwindow.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/settings.cpp \
    src/snackbar.cpp \
    src/swipefilter.cpp \
    src/synclogview.cpp \
    src/syncmanager.cpp

FORMS += \
    src/mainwindow.ui

RESOURCES += \
    resources/resources.qrc

win32 {
    RC_FILE = Fahrtenbuch_resource.rc
}

android {
    HEADERS += include/exportbridge.h
    SOURCES += src/exportbridge.cpp

    # Qt Quick + QML-Kalender (nur Android-Build)
    # QQuickView benoetigt quick + quickcontrols2 (kein quickwidgets)
    QT += quick quickcontrols2 quickwidgets

    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/Android

    # OpenSSL für HTTPS (ORS-Entfernungsberechnung)
    # Die .so-Dateien liegen im Projektordner unter openssl/arm64-v8a/
    # Einmalig kopieren: C:\android_openssl\ssl_3\arm64-v8a\libssl_3.so
    #                                                        libcrypto_3.so
    ANDROID_EXTRA_LIBS += \
        $$PWD/openssl/arm64-v8a/libssl_3.so \
        $$PWD/openssl/arm64-v8a/libcrypto_3.so \
        $$PWD/openssl/x86_64/libssl_3.so \
        $$PWD/openssl/x86_64/libcrypto_3.so

    DISTFILES += \
        Android/AndroidManifest.xml \
        Android/build.gradle \
        Android/gradle.properties \
        Android/gradlew \
        Android/gradlew.bat \
        Android/gradle/wrapper/gradle-wrapper.jar \
        Android/gradle/wrapper/gradle-wrapper.properties \
        Android/res/drawable/icon.png \
        Android/res/values/libs.xml \
        Android/res/xml/qtprovider_paths.xml \
        Android/res/xml/network_security_config.xml
}

DISTFILES += \
    Android/AndroidManifest.xml \
    Android/build.gradle \
    Android/gradle.properties \
    Android/gradle/wrapper/gradle-wrapper.jar \
    Android/gradle/wrapper/gradle-wrapper.properties \
    Android/gradlew \
    Android/gradlew.bat \
    Android/res/values/libs.xml \
    Android/res/xml/qtprovider_paths.xml \
    keystore.properties.template
