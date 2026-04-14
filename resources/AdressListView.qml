// AdressListView.qml
// Ersetzt QTableWidget auf Android fuer Adressen.
// Kommunikation ueber "listBridge" (AdressListBridge, via QQmlContext).
// Aussehen und Struktur identisch mit FahrtListView.qml.

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    // ── Farben (identisch mit FahrtListView) ──────────────────────────────────
    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F2F4F5"
    readonly property color clCard    : "#FFFFFF"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clSub     : "#72787E"
    readonly property color clDivider : "#DCE4E9"
    readonly property color clFab     : "#F39C12"

    // safeBottom: Höhe der Android-Navigationsleiste
    // Bei compileSdk=34 (kein Edge-to-Edge) verwaltet Android den Abstand selbst → 0
    readonly property int safeBottom: 0

    // dp: density-independent pixels (wie Android).
    // 1 dp = 1 px bei 160 dpi (mdpi). Auf höherdichten Geräten skaliert alles korrekt.
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    // ── Hintergrund ───────────────────────────────────────────────────────────
    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── Sortier-Leiste ────────────────────────────────────────────────────────
    Rectangle {
        id: sortBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 44 * dp; color: root.clCard
        z: 2

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1; color: root.clDivider
        }

        Row {
            anchors { fill: parent; leftMargin: 10; rightMargin: 8;
                      topMargin: 6; bottomMargin: 6 }
            spacing: 6

            SortChip {
                label: qsTr("Bezeichnung")
                field: "bezeichnung"
                active: listBridge.sortField === "bezeichnung"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("bezeichnung",
                    listBridge.sortField === "bezeichnung" ? !listBridge.sortAsc : true)
            }
            SortChip {
                label: qsTr("Ort")
                field: "ort"
                active: listBridge.sortField === "ort"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("ort",
                    listBridge.sortField === "ort" ? !listBridge.sortAsc : true)
            }
        }
    }

    // ── Liste ─────────────────────────────────────────────────────────────────
    ListView {
        id: listView
        anchors {
            top:    sortBar.bottom
            bottom: parent.bottom
            left:   parent.left
            right:  parent.right
        }
        bottomMargin: 80 + root.safeBottom   // Platz fuer FAB
        clip: true
        spacing: 0

        model: listBridge.adressen

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            width:  listView.width
            height: card.height + 1   // +1 fuer Trennlinie

            // Trennlinie
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.clDivider
            }

            // Karte
            Rectangle {
                id: card
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: cardContent.implicitHeight + 20
                color: tapHandler.pressed ? "#EDF4F9" : root.clCard

                Column {
                    id: cardContent
                    anchors {
                        left:  parent.left;  leftMargin:  16
                        right: parent.right; rightMargin: 28
                        top:   parent.top;   topMargin:   10
                    }
                    spacing: 3

                    // Zeile 1: Bezeichnung
                    Text {
                        width: parent.width
                        text:  modelData.bezeichnung
                        font.pixelSize: 14 * dp; font.bold: true
                        color: root.clPrimary
                        elide: Text.ElideRight
                    }

                    // Zeile 2: Adresse (nur wenn vorhanden)
                    Text {
                        width: parent.width
                        text:  modelData.adresse
                        font.pixelSize: 14 * dp; color: root.clSub
                        elide: Text.ElideRight
                        visible: modelData.adresse.length > 0
                    }
                }

                MouseArea {
                    id: tapHandler
                    anchors.fill: parent
                    onClicked: listBridge.requestEdit(modelData.id)
                }
            }
        }

        // Leerer Zustand
        Item {
            anchors.centerIn: parent
            visible: listView.count === 0
            Column {
                anchors.centerIn: parent
                spacing: 12
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Keine Adressen")
                    font.pixelSize: 20 * dp; color: root.clSub
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Tippe auf + um eine neue Adresse anzulegen")
                    font.pixelSize: 14 * dp; color: root.clSub
                    opacity: 0.7
                }
            }
        }
    }

    // ── FAB ───────────────────────────────────────────────────────────────────
    Rectangle {
        id: fab
        width: 60 * dp; height: 60 * dp; radius: 30 * dp
        color: fabTap.pressed ? "#d4860f" : root.clFab

        anchors {
            right:  parent.right;  rightMargin:  20
            bottom: parent.bottom; bottomMargin: 20 + root.safeBottom
        }
        z: 10

        // + Symbol
        Rectangle {
            anchors.centerIn: parent
            width: 26; height: 3; radius: 1
            color: "white"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 3; height: 26; radius: 1
            color: "white"
        }

        // Schatten
        Rectangle {
            anchors { fill: parent; margins: -3 }
            radius: parent.radius + 3
            color: "transparent"
            border.color: "#30000000"; border.width: 3
            z: -1
        }

        MouseArea {
            id: fabTap
            anchors.fill: parent
            onClicked: listBridge.requestAdd()
        }
    }

    // ── SortChip Inline-Komponente (identisch mit FahrtListView) ─────────────
    component SortChip: Rectangle {
        id: chip
        property string label:  ""
        property string field:  ""
        property bool   active: false
        property bool   asc:    true
        signal clicked()

        height: 32 * dp
        width:  chipRow.width + 20
        radius: 14
        color:  active ? root.clPrimary : root.clDivider

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 3

            Text {
                text: chip.label
                font.pixelSize: 13 * dp
                color: chip.active ? "white" : root.clOnSurf
                anchors.verticalCenter: parent.verticalCenter
            }

            // Pfeil-Indikator (nur wenn aktiv)
            Text {
                visible: chip.active
                text: chip.asc ? "^" : "v"
                font.pixelSize: 11 * dp; font.bold: true
                color: "white"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        TapHandler { onTapped: chip.clicked() }
    }
}
