// AboutView.qml — MD3 Info-Dialog für Android

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root
    signal closeRequested()

    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F2F4F5"
    readonly property color clCard    : "#FFFFFF"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clSub     : "#72787E"
    readonly property color clDivider : "#DCE4E9"

    // safeBottom: Höhe der Android-Navigationsleiste
    // Bei compileSdk=34 (kein Edge-to-Edge) verwaltet Android den Abstand selbst → 0
    readonly property int safeBottom: 0

    // dp: density-independent pixels (wie Android).
    // 1 dp = 1 px bei 160 dpi (mdpi). Auf höherdichten Geräten skaliert alles korrekt.
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── App-Bar ────────────────────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 56 * dp; color: root.clPrimary; z: 2


        Text {
            anchors.centerIn: parent
            text: aboutAppName || "Mileage Log"
            color: "white"; font.pixelSize: 18 * dp; font.bold: true
        }
    }

    // ── Inhalt ─────────────────────────────────────────────────────────────
    ScrollView {
        anchors {
            top: topBar.bottom; bottom: btnBar.top
            left: parent.left; right: parent.right
        }
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            width: parent.width
            topPadding: 32; bottomPadding: 24; spacing: 0

            // App-Icon / Titel
            Image {
                id: appIcon
                anchors.horizontalCenter: parent.horizontalCenter
                width: 96; height: 96
                source: "qrc:/fahrtenbuch.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Item { width: 1; height: 16 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: aboutAppName || "Mileage Log"
                font.pixelSize: 24 * dp; font.bold: true; color: root.clPrimary
            }

            Item { width: 1; height: 8 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Version " + appVersion
                font.pixelSize: 14 * dp; color: root.clSub
            }

            Item { width: 1; height: 32 }

            // Info-Karte
            Rectangle {
                x: 24; width: parent.width - 48
                color: root.clCard; radius: 12
                height: infoCol.implicitHeight + 32

                Column {
                    id: infoCol
                    anchors { left: parent.left; right: parent.right
                              top: parent.top; margins: 16 }
                    spacing: 12

                    InfoRow { label: (aboutLabelQt || "Qt version");      value: qtVersion }
                    InfoRow { label: (aboutLabelPlatform || "Platform");  value: aboutPlatform || "Windows / Android" }
                    InfoRow { label: (aboutLabelDb || "Database");        value: aboutDatabase || "SQLite 3 (WAL)" }
                    InfoRow { label: (aboutLabelDist || "Distance");      value: aboutDistance || "ORS fastest route" }
                }
            }

            Item { width: 1; height: 24 }

            Text {
                x: 24; width: parent.width - 48
                text: aboutDescription || ""
                font.pixelSize: 14 * dp; color: root.clSub
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item { width: 1; height: 16 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: aboutCopyright || ""
                font.pixelSize: 12 * dp; color: root.clSub
            }

            Item { width: 1; height: 12 }

            Text {
                x: 24; width: parent.width - 48
                text: aboutLicenseText || ""
                font.pixelSize: 11 * dp; color: root.clSub
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item { width: 1; height: 10 }

            // LGPL-Link (anklickbar)
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4

                Text {
                    text: aboutLicenseLabel || "License:"
                    font.pixelSize: 11 * dp; color: root.clSub
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "GNU LGPL v3"
                    font.pixelSize: 11 * dp; color: root.clPrimary
                    font.underline: true
                    anchors.verticalCenter: parent.verticalCenter
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Qt.openUrlExternally(
                            "https://www.gnu.org/licenses/lgpl-3.0.html")
                    }
                }
            }

            Item { width: 1; height: 6 }

            // Qt-Quellcode-Link (LGPL: Nutzer muss Qt neu linken können)
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4

                Text {
                    text: aboutSourceLabel || "Qt source code:"
                    font.pixelSize: 11 * dp; color: root.clSub
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "download.qt.io"
                    font.pixelSize: 11 * dp; color: root.clPrimary
                    font.underline: true
                    anchors.verticalCenter: parent.verticalCenter
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Qt.openUrlExternally("https://download.qt.io")
                    }
                }
            }

            Item { width: 1; height: 12 }
        }
    }

    // ── Button-Leiste ──────────────────────────────────────────────────────
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 64 * dp + root.safeBottom; color: root.clCard

        Rectangle {
            width: parent.width; height: 1; color: root.clDivider
            anchors.top: parent.top
        }

        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: 16; rightMargin: 16; topMargin: 10 }
            height: 52; radius: 10
            color: okTap.pressed ? "#004E7A" : root.clPrimary

            Text {
                anchors.centerIn: parent
                text: "OK"; color: "white"
                font.pixelSize: 15 * dp; font.bold: true
            }

            MouseArea {
                id: okTap
                anchors.fill: parent
                onClicked: root.closeRequested()
            }
        }
    }

    // ── Inline-Komponente ──────────────────────────────────────────────────
    component InfoRow: Item {
        property string label: ""
        property string value: ""
        width: parent.width; height: 28 * dp

        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: label + ":"
            font.pixelSize: 13 * dp; color: root.clSub
        }
        Text {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            text: value
            font.pixelSize: 13 * dp; font.bold: true; color: root.clOnSurf
        }
    }
}
