// HelpView.qml — MD3 Hilfe-Dialog für Android

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
            text: qsTr("Benutzeranleitung")
            color: "white"; font.pixelSize: 18 * dp; font.bold: true
        }
    }

    // ── Liste der Hilfesektionen ───────────────────────────────────────────
    ListView {
        id: listView
        anchors { top: topBar.bottom; bottom: btnBar.top; left: parent.left; right: parent.right }
        clip: true
        bottomMargin: 8
        spacing: 0

        model: helpSections

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            width: listView.width
            height: sectionCard.height + 12

            Rectangle {
                id: sectionCard
                anchors { top: parent.top; topMargin: 12; left: parent.left; leftMargin: 16; right: parent.right; rightMargin: 16 }
                radius: 12; color: root.clCard
                height: cardCol.implicitHeight + 24

                Column {
                    id: cardCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 8

                    // Titel
                    Text {
                        width: parent.width
                        text: modelData.title
                        font.pixelSize: 14 * dp; font.bold: true; color: root.clPrimary
                    }

                    // Trennlinie
                    Rectangle {
                        width: parent.width; height: 1; color: root.clDivider
                    }

                    // Inhalt (Zeilenumbrüche \n → mehrere Text-Zeilen)
                    Repeater {
                        model: modelData.body.split("\n")
                        Text {
                            width: cardCol.width
                            text: modelData
                            font.pixelSize: 13 * dp; color: root.clOnSurf
                            wrapMode: Text.WordWrap
                            visible: modelData.trim().length > 0
                        }
                    }
                }
            }
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
            color: closeBtnTap.pressed ? "#004E7A" : root.clPrimary

            Text {
                anchors.centerIn: parent
                text: qsTr("Schließen"); color: "white"
                font.pixelSize: 15 * dp; font.bold: true
            }

            MouseArea {
                id: closeBtnTap
                anchors.fill: parent
                onClicked: root.closeRequested()
            }
        }
    }
}
