// AdressFormDialog.qml
// Vollbild-Formular fuer Neue Adresse / Adresse bearbeiten auf Android.
// Kommunikation ausschliesslich ueber das "bridge"-Objekt (AdressFormBridge).

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    // safeBottom: Höhe der Android-Navigationsleiste
    // Bei compileSdk=34 (kein Edge-to-Edge) verwaltet Android den Abstand selbst → 0
    readonly property int safeBottom: 0

    // dp: density-independent pixels (wie Android).
    // 1 dp = 1 px bei 160 dpi (mdpi). Auf höherdichten Geräten skaliert alles korrekt.
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0
    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F8FAFB"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clOutline : "#72787E"
    readonly property color clDanger  : "#BA1A1A"
    readonly property color clWhite   : "#FFFFFF"

    readonly property bool saveOk: ortInput.inputText.trim().length > 0

    readonly property int safeAreaBottom: Math.max(0,
        Screen.height - Screen.desktopAvailableHeight)
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
            text:  bridge.isNew ? qsTr("Neue Adresse") : qsTr("Adresse bearbeiten")
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

            // ── Bezeichnung ────────────────────────────────────────
            FieldLabel { text: qsTr("Bezeichnung") }
            FieldInput {
                id: bezInput
                initText:    bridge.initBez
                placeholder: qsTr("z.B. Büro, Zuhause")
            }
            FieldSpacer {}

            // ── Strasse ────────────────────────────────────────────
            FieldLabel { text: qsTr("Straße") }
            FieldInput { id: strInput; initText: bridge.initStr }
            FieldSpacer {}

            // ── Hausnummer ─────────────────────────────────────────
            FieldLabel { text: qsTr("Hausnr.") }
            FieldInput {
                id: hnrInput
                initText: bridge.initHnr
                hints:    Qt.ImhLatinOnly
            }
            FieldSpacer {}

            // ── PLZ ────────────────────────────────────────────────
            FieldLabel { text: qsTr("PLZ") }
            FieldInput {
                id: plzInput
                initText: bridge.initPlz
                hints:    Qt.ImhDigitsOnly
            }
            FieldSpacer {}

            // ── Ort ────────────────────────────────────────────────
            FieldLabel { text: qsTr("Ort *") }
            FieldInput {
                id: ortInput
                initText:    bridge.initOrt
                placeholder: qsTr("Pflichtfeld")
            }
            FieldSpacer {}

            Text {
                x: 16; text: qsTr("* Pflichtfelder")
                color: root.clOutline; font.pixelSize: 12 * dp
            }

            Item { width: 1; height: 16 }
        }
    }

    // ── Button-Leiste ────────────────────────────────────────────────
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: (bridge.errorText.length > 0 ? 28 * dp : 0) + 64 * dp + root.safeBottom
        color: root.clWhite

        // Fehlermeldung (z.B. Duplikat)
        Rectangle {
            visible: bridge.errorText.length > 0
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 28 * dp
            color: "#FDECEA"
            Text {
                anchors.centerIn: parent
                text: bridge.errorText
                font.pixelSize: 12 * dp
                color: "#B71C1C"
            }
        }

        Rectangle {
            width: parent.width; height: 1; color: "#DCE4E9"
            anchors.top: parent.top
        }

        Row {
            anchors {
                left: parent.left; right: parent.right
                bottom: parent.bottom; bottomMargin: root.safeBottom
                topMargin: 10; leftMargin: 10; rightMargin: 10
            }
            height: 52
            spacing: 8

            // Loeschen (nur bei bestehender Adresse)
            Rectangle {
                visible: !bridge.isNew
                width:   visible ? (parent.width - 16) / 3 : 0
                height: 52; radius: 10
                color: delArea.pressed ? "#8B0000" : root.clDanger
                Text { anchors.centerIn: parent; text: qsTr("Löschen")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true }
                MouseArea {
                    id: delArea; anchors.fill: parent
                    onClicked: bridge.deleteAdresse()
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
                    onClicked: bridge.save(
                        bezInput.inputText,
                        strInput.inputText,
                        hnrInput.inputText,
                        plzInput.inputText,
                        ortInput.inputText
                    )
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
        property int    hints:       Qt.ImhNone
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
                inputMethodHints: fi.hints

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
