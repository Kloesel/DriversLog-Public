// ExportView.qml
// Export-Dialog für Android: CSV und PDF erzeugen und teilen.
// Kommunikation über "exportBridge" (ExportBridge via QQmlContext).

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F8FAFB"
    readonly property color clCard    : "#FFFFFF"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clOutline : "#72787E"
    readonly property color clDivider : "#DCE4E9"
    readonly property color clDanger  : "#BA1A1A"
    readonly property color clWhite   : "#FFFFFF"

    readonly property int  safeBottom: 0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    // Bridge-Guard
    property bool hasBridge: typeof exportBridge !== "undefined" && exportBridge !== null

    Component.onCompleted: {
        if (hasBridge) exportBridge.reset()
    }

    Connections {
        target: hasBridge ? exportBridge : null
        function onBusyChanged() { /* busy state handled by button colors */ }
    }

    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── Scrollbarer Inhalt ────────────────────────────────────────────
    ScrollView {
        anchors { top: parent.top; bottom: btnBar.top; left: parent.left; right: parent.right }
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            width: parent.width
            topPadding: 16
            bottomPadding: 24
            spacing: 0

            // ── Filter ────────────────────────────────────────────────
            SectionHeader { text: qsTr("Zeitraum") }

            // Monat
            FieldLabel { text: qsTr("Monat") }
            FieldCombo {
                id: monatCb
                comboModel: hasBridge ? exportBridge.monatModel : []
                initId:     hasBridge ? exportBridge.initMonat  : 0
            }
            FieldSpacer {}

            // Jahr
            FieldLabel { text: qsTr("Jahr") }
            Rectangle {
                x: 16; width: parent.width - 32; height: 52 * dp
                radius: 8; color: root.clCard
                border.color: jahrInput.activeFocus ? root.clPrimary : root.clOutline
                border.width: jahrInput.activeFocus ? 2 : 1
                TextInput {
                    id: jahrInput
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    text: hasBridge ? exportBridge.initJahr.toString() : ""
                    font.pixelSize: 16 * dp; color: root.clOnSurf
                    verticalAlignment: TextInput.AlignVCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                }
            }
            FieldSpacer {}

            // ── Was exportieren? ──────────────────────────────────────
            SectionHeader { text: qsTr("Inhalt") }

            CheckRow { id: chkFahrten;         label: qsTr("Fahrten");         checked: true  }
            CheckRow { id: chkAdressen;        label: qsTr("Adressen");        checked: false }
            CheckRow { id: chkZusammenfassung; label: qsTr("Zusammenfassung"); checked: false }

            FieldSpacer {}
        }
    }

    // ── Button-Leiste ─────────────────────────────────────────────────
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 64 * dp + root.safeBottom
        color: root.clWhite

        Rectangle {
            width: parent.width; height: 1; color: root.clDivider
            anchors.top: parent.top
        }

        Row {
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                topMargin: 8; leftMargin: 10; rightMargin: 10
            }
            height: 52; spacing: 8

            // Schließen
            Rectangle {
                width: (parent.width - 16) / 3; height: 52; radius: 10
                color: canArea.pressed ? "#C4CDD4" : root.clDivider
                Text { anchors.centerIn: parent; text: qsTr("Schließen")
                    color: root.clOnSurf; font.pixelSize: 14 * dp }
                MouseArea {
                    id: canArea; anchors.fill: parent
                    onClicked: if (hasBridge) exportBridge.closeRequested()
                }
            }

            // CSV-Export
            Rectangle {
                width: (parent.width - 16) / 3; height: 52; radius: 10
                color: !hasBridge || exportBridge.busy ? root.clOutline
                     : csvArea.pressed ? "#004E7A" : root.clPrimary
                Text { anchors.centerIn: parent; text: qsTr("📥 CSV")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true }
                MouseArea {
                    id: csvArea; anchors.fill: parent
                    enabled: hasBridge && !exportBridge.busy
                    onClicked: exportBridge.exportCsv(
                        monatCb.currentId,
                        parseInt(jahrInput.text) || 0,
                        chkFahrten.checked,
                        chkZusammenfassung.checked,
                        chkAdressen.checked)
                }
            }

            // PDF-Export
            Rectangle {
                width: (parent.width - 16) / 3; height: 52; radius: 10
                color: !hasBridge || exportBridge.busy ? root.clOutline
                     : pdfArea.pressed ? "#8B0000" : root.clDanger
                Text { anchors.centerIn: parent; text: qsTr("📄 PDF")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true }
                MouseArea {
                    id: pdfArea; anchors.fill: parent
                    enabled: hasBridge && !exportBridge.busy
                    onClicked: exportBridge.exportPdf(
                        monatCb.currentId,
                        parseInt(jahrInput.text) || 0,
                        chkFahrten.checked,
                        chkZusammenfassung.checked,
                        chkAdressen.checked)
                }
            }
        }
    }

    // ── Wiederverwendbare Inline-Komponenten ──────────────────────────

    component SectionHeader: Rectangle {
        property string text: ""
        x: 0; width: parent.width; height: 36 * dp
        color: "#EDF4F9"
        Text {
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: parent.text
            font.pixelSize: 12 * dp; font.bold: true; color: root.clPrimary
        }
    }

    component FieldLabel: Text {
        x: 16; width: parent.width - 32
        font.pixelSize: 12 * dp; color: root.clOutline
        topPadding: 8; bottomPadding: 4
    }

    component FieldSpacer: Item { width: 1; height: 12 }

    component CheckRow: Item {
        property string label:   ""
        property bool   checked: false
        x: 16; width: parent.width - 32; height: 48 * dp
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Rectangle {
                width: 24; height: 24; radius: 4
                border.color: parent.parent.checked ? root.clPrimary : root.clOutline
                border.width: 2
                color: parent.parent.checked ? root.clPrimary : root.clWhite
                Text {
                    anchors.centerIn: parent; text: "✓"
                    color: root.clWhite; font.pixelSize: 14 * dp
                    visible: parent.parent.parent.checked
                }
            }
            Text {
                text: parent.parent.label
                font.pixelSize: 15 * dp; color: root.clOnSurf
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: parent.checked = !parent.checked
        }
    }

    component FieldCombo: ComboBox {
        id: fc
        x: 16; width: parent.width - 32; height: 52
        property var comboModel: []
        property int initId:     0
        property int currentId:  currentIndex >= 0 && currentIndex < comboModel.length
                                  ? comboModel[currentIndex].id : 0
        model:    comboModel
        textRole: "text"
        font.pixelSize: 15 * dp

        Component.onCompleted: {
            for (var i = 0; i < comboModel.length; ++i) {
                if (comboModel[i].id === initId) { currentIndex = i; break }
            }
        }
    }
}
