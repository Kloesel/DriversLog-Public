// FahrerFormDialog.qml
// Vollbild-Formular fuer Neuer Fahrer / Fahrer bearbeiten auf Android.
// Kommunikation ausschliesslich ueber das "bridge"-Objekt (FahrerFormBridge).

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    readonly property int   safeBottom: 0
    readonly property real  dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0
    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F8FAFB"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clOutline : "#72787E"
    readonly property color clDanger  : "#BA1A1A"
    readonly property color clWhite   : "#FFFFFF"

    readonly property bool saveOk: nameInput.inputText.trim().length > 0

    readonly property real kbHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height / Screen.devicePixelRatio
        : 0

    Rectangle { anchors.fill: parent; color: root.clSurface }

    Item {
        id: sheet
        anchors.fill: parent

    // ── Kopfzeile ────────────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 56 * dp; color: root.clPrimary; z: 2
        Text {
            anchors.centerIn: parent
            text:  bridge.isNew ? qsTr("Neuer Fahrer") : qsTr("Fahrer bearbeiten")
            color: root.clWhite; font.pixelSize: 18 * dp; font.bold: true
        }
    }

    // ── Scrollbares Formular ─────────────────────────────────────────
    ScrollView {
        id: sv
        anchors {
            top:    topBar.bottom
            bottom: btnBar.top
            left:   parent.left
            right:  parent.right
        }
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            width:         sv.availableWidth
            topPadding:    16
            bottomPadding: 24 + root.safeBottom
            spacing:       0

            // ── Name ───────────────────────────────────────────────
            FieldLabel { text: qsTr("Name *") }
            FieldInput {
                id: nameInput
                initText:    bridge.initName
                placeholder: qsTr("Pflichtfeld")
            }
            FieldSpacer {}

            Item { width: 1; height: 16 }
        }
    }

    // ── Button-Leiste ────────────────────────────────────────────────
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 64 * dp + root.safeBottom
        color: root.clWhite

        Rectangle {
            width: parent.width; height: 1; color: "#DCE4E9"
            anchors.top: parent.top
        }

        Row {
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                topMargin: 10; leftMargin: 10; rightMargin: 10
            }
            height: 52
            spacing: 8

            // Loeschen (nur bei bestehendem Fahrer)
            Rectangle {
                visible: !bridge.isNew
                width:   visible ? (parent.width - 16) / 3 : 0
                height: 52; radius: 10
                color: bridge.isDefault  ? root.clOutline
                     : delArea.pressed   ? "#8B0000"
                     :                     root.clDanger
                Text { anchors.centerIn: parent; text: qsTr("Löschen")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true }
                MouseArea {
                    id: delArea; anchors.fill: parent
                    enabled: !bridge.isDefault
                    onClicked: bridge.deleteFahrer()
                }
            }

            // Abbrechen
            Rectangle {
                width:  bridge.isNew ? (parent.width - 8) / 2 : (parent.width - 16) / 3
                height: 52; radius: 10
                color: canArea.pressed ? "#C4CDD4" : "#DCE4E9"
                Text { anchors.centerIn: parent; text: qsTr("Abbrechen")
                    color: root.clOnSurf; font.pixelSize: 14 * dp }
                MouseArea {
                    id: canArea; anchors.fill: parent
                    onClicked: { Qt.inputMethod.hide(); bridge.cancel() }
                }
            }

            // Speichern
            Rectangle {
                width:  bridge.isNew ? (parent.width - 8) / 2 : (parent.width - 16) / 3
                height: 52; radius: 10
                color: !root.saveOk    ? root.clOutline
                     : savArea.pressed ? "#004E7A"
                     :                   root.clPrimary
                Text { anchors.centerIn: parent; text: qsTr("Speichern")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true }
                MouseArea {
                    id: savArea; anchors.fill: parent
                    enabled: root.saveOk
                    onClicked: bridge.save(nameInput.inputText)
                }
            }
        }
    }

    } // end sheet

    // ── Wiederverwendbare Inline-Komponenten ─────────────────────────

    component FieldLabel: Text {
        x: 16; width: parent.width - 32
        font.pixelSize: 12 * dp; color: root.clOutline
        topPadding: 4; bottomPadding: 4
    }

    component FieldSpacer: Item { width: 1; height: 14 }

    component FieldInput: Item {
        id: fi
        x: 16; width: parent.width - 32; height: 52

        property string initText:    ""
        property string placeholder: ""
        property alias  inputText:   ti.text

        Rectangle {
            anchors.fill: parent
            radius: 8; color: root.clWhite
            border.color: ti.activeFocus ? root.clPrimary : root.clOutline
            border.width: ti.activeFocus ? 2 : 1

            TextInput {
                id: ti
                anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                text: fi.initText
                font.pixelSize: 16 * dp; color: root.clOnSurf
                verticalAlignment: TextInput.AlignVCenter

                Text {
                    anchors { fill: parent; leftMargin: 0 }
                    text: fi.placeholder
                    font.pixelSize: 16 * dp; color: root.clOutline; opacity: 0.6
                    verticalAlignment: Text.AlignVCenter
                    visible: ti.text.length === 0 && !ti.activeFocus
                }
            }
        }
    }
}
