// FahrtListView.qml
// Ersetzt QTableWidget auf Android.
// Kommunikation über "listBridge" (FahrtListBridge, via QQmlContext).
// Kein Qt.labs.calendar, kein externes Modul.

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    // Null-Guard: verhindert TypeError-Warnings während App-Neustart bei Sprachumschaltung
    readonly property bool bridgeReady: typeof listBridge !== "undefined" && listBridge !== null

    // ── Farben ────────────────────────────────────────────────────────
    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F2F4F5"
    readonly property color clCard    : "#FFFFFF"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clSub     : "#72787E"
    readonly property color clDivider : "#DCE4E9"
    readonly property color clFab     : "#F39C12"

    // Safe-area für Navigationsleiste
    // safeBottom: Höhe der Android-Navigationsleiste
    // Bei compileSdk=34 (kein Edge-to-Edge) verwaltet Android den Abstand selbst → 0
    readonly property int safeBottom: 0

    // dp: density-independent pixels (wie Android).
    // 1 dp = 1 px bei 160 dpi (mdpi). Auf höherdichten Geräten skaliert alles korrekt.
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    // ── Hintergrund ───────────────────────────────────────────────────
    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── Sortier-Leiste ────────────────────────────────────────────────
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
                label: qsTr("Datum")
                field: "datum"
                active: listBridge.sortField === "datum"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("datum",
                    listBridge.sortField === "datum" ? !listBridge.sortAsc : false)
            }
            SortChip {
                label: qsTr("Start")
                field: "start_adresse_id"
                active: listBridge.sortField === "start_adresse_id"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("start_adresse_id",
                    listBridge.sortField === "start_adresse_id" ? !listBridge.sortAsc : true)
            }
            SortChip {
                label: qsTr("Ziel")
                field: "ziel_adresse_id"
                active: listBridge.sortField === "ziel_adresse_id"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("ziel_adresse_id",
                    listBridge.sortField === "ziel_adresse_id" ? !listBridge.sortAsc : true)
            }
            SortChip {
                label: qsTr("km")
                field: "entfernung"
                active: listBridge.sortField === "entfernung"
                asc:    listBridge.sortAsc
                onClicked: listBridge.setSort("entfernung",
                    listBridge.sortField === "entfernung" ? !listBridge.sortAsc : false)
            }
        }
    }

    // ── Liste ─────────────────────────────────────────────────────────
    ListView {
        id: listView
        anchors {
            top:    sortBar.bottom
            bottom: parent.bottom
            left:   parent.left
            right:  parent.right
        }
        bottomMargin: 80 + root.safeBottom   // Platz für FAB + Navigationsleiste
        clip: true
        spacing: 0

        model: listBridge.fahrten

        ScrollBar.vertical: ScrollBar {
            id: vScrollBar
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            width:  listView.width
            height: card.height + 1   // +1 für Trennlinie

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
                        right: parent.right; rightMargin: 28   // 28 = 16 normal + 12 für ScrollBar
                        top:   parent.top;   topMargin:   10
                    }
                    spacing: 3

                    // Zeile 1: Datum | Fahrer (optional) | km
                    Row {
                        width: parent.width
                        spacing: 0

                        Text {
                            id: datumText
                            text: modelData.datum
                            font.pixelSize: 13 * dp; font.bold: true
                            color: root.clPrimary
                            width: (bridgeReady && listBridge.mehrerefahrer) && modelData.fahrer.length > 0
                                   ? implicitWidth + 8
                                   : parent.width - kmText.width
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: (bridgeReady && listBridge.mehrerefahrer) && modelData.fahrer.length > 0
                            text: modelData.fahrer
                            font.pixelSize: 13 * dp
                            color: root.clSub
                            width: parent.width - datumText.width - kmText.width
                            elide: Text.ElideRight
                        }
                        Text {
                            id: kmText
                            text: modelData.km + " km"
                            font.pixelSize: 13 * dp; font.bold: true
                            color: root.clOnSurf
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Zeile 2: Start (fett wenn nach Start sortiert)
                    Text {
                        width: parent.width
                        text: modelData.start
                        font.pixelSize: 14 * dp
                        font.bold: listBridge.sortField === "start_adresse_id"
                        color: listBridge.sortField === "start_adresse_id"
                               ? root.clPrimary : root.clOnSurf
                        elide: Text.ElideRight
                    }

                    // Zeile 3: Ziel (fett wenn nach Ziel sortiert)
                    Text {
                        width: parent.width
                        text: modelData.ziel
                        font.pixelSize: 14 * dp
                        font.bold: listBridge.sortField === "ziel_adresse_id"
                        color: listBridge.sortField === "ziel_adresse_id"
                               ? root.clPrimary : root.clOnSurf
                        elide: Text.ElideRight
                    }

                    // Zeile 3: Richtungssymbol + Bemerkung
                    // Symbol immer sichtbar: hz=true → beide Bögen, hz=false → nur oberer
                    Row {
                        visible: true
                        width: parent.width
                        spacing: 8

                        // Richtungssymbol (Recycling-/Kreispfeil-Stil)
                        // Gezeichnet als gefüllte Pfeilformen entlang zweier Kreisbögen.
                        // Oberer Bogen: 200°→340° (UZS durch oben), Spitze bei 340°
                        // Unterer Bogen: 20°→160° (UZS durch unten), Spitze bei 160°
                        // Für hz=false nur der obere Bogen gezeichnet.
                        Canvas {
                            width: 22; height: 22

                            // Neuzeichnen wenn sich hz ändert
                            property bool hz: modelData.hz
                            onHzChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)

                                var cx  = width  / 2
                                var cy  = height / 2
                                var r   = 7.5    // Bogenradius
                                var lw  = 2.2    // Linienbreite Bogen
                                var aw  = 4.0    // Pfeilspitzen-Größe

                                ctx.strokeStyle = root.clPrimary
                                ctx.fillStyle   = root.clPrimary
                                ctx.lineWidth   = lw
                                ctx.lineCap     = "butt"
                                ctx.lineJoin    = "miter"

                                // Gefüllte Pfeilspitze am Ende (theta_end) eines UZS-Bogens.
                                // UZS-Tangente bei θ: t=(-sinθ, cosθ)  [vorwärts]
                                // Normale (auswärts):  n=(cosθ, sinθ)
                                function arrowHead(theta) {
                                    var tx  = cx + r * Math.cos(theta)
                                    var ty  = cy + r * Math.sin(theta)
                                    var tdx = -Math.sin(theta)   // Tangente vorwärts
                                    var tdy =  Math.cos(theta)
                                    var ndx =  Math.cos(theta)   // Normale auswärts
                                    var ndy =  Math.sin(theta)
                                    // Dreieck: Spitze vorwärts, Basis zurück ± Normale
                                    ctx.beginPath()
                                    ctx.moveTo(tx + aw*tdx,
                                               ty + aw*tdy)
                                    ctx.lineTo(tx - aw*0.5*tdx + aw*0.75*ndx,
                                               ty - aw*0.5*tdy + aw*0.75*ndy)
                                    ctx.lineTo(tx - aw*0.5*tdx - aw*0.75*ndx,
                                               ty - aw*0.5*tdy - aw*0.75*ndy)
                                    ctx.closePath()
                                    ctx.fill()
                                }

                                // ── Oberer Bogen (immer gezeichnet) ──────────
                                // 200° → 340°, UZS durch 270° (oben im Screen)
                                var s1 = 200 * Math.PI / 180
                                var e1 = 340 * Math.PI / 180
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, s1, e1, false)
                                ctx.stroke()
                                arrowHead(e1)

                                // ── Unterer Bogen (nur hz=true) ───────────────
                                // 20° → 160°, UZS durch 90° (unten im Screen)
                                if (modelData.hz) {
                                    var s2 = 20  * Math.PI / 180
                                    var e2 = 160 * Math.PI / 180
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r, s2, e2, false)
                                    ctx.stroke()
                                    arrowHead(e2)
                                }
                            }
                        }

                        Text {
                            visible: modelData.bemerkung.length > 0
                            text: modelData.bemerkung
                            font.pixelSize: 14 * dp; color: root.clSub
                            width: parent.width - 30
                            elide: Text.ElideRight
                        }
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
                    text: qsTr("Keine Fahrten")
                    font.pixelSize: 20 * dp; color: root.clSub
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Tippe auf + um eine neue Fahrt anzulegen")
                    font.pixelSize: 14 * dp; color: root.clSub
                    opacity: 0.7
                }
            }
        }
    }

    // ── FAB ───────────────────────────────────────────────────────────
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

    // ── SortChip Inline-Komponente ────────────────────────────────────
    component SortChip: Rectangle {
        id: chip
        property string label: ""
        property string field: ""
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
