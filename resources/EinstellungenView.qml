// EinstellungenView.qml
// Einstellungs-Formular fuer Android.
// Kommunikation ueber "settingsBridge" (EinstellungenBridge, via QQmlContext).

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    // ── Farben ────────────────────────────────────────────────────────────────
    readonly property color clPrimary : "#006493"
    readonly property color clSurface : "#F2F4F5"
    readonly property color clCard    : "#FFFFFF"
    readonly property color clOnSurf  : "#1A1C1E"
    readonly property color clOutline : "#72787E"
    readonly property color clDivider : "#DCE4E9"

    // safeBottom: Höhe der Android-Navigationsleiste
    // Bei compileSdk=34 (kein Edge-to-Edge) verwaltet Android den Abstand selbst → 0
    readonly property int safeBottom: 0

    // dp: density-independent pixels (wie Android).
    // 1 dp = 1 px bei 160 dpi (mdpi). Auf höherdichten Geräten skaliert alles korrekt.
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0
    readonly property real kbHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height / Screen.devicePixelRatio
        : 0

    function findIdx(model, id) {
        for (var i = 0; i < model.length; i++)
            if (model[i].id === id) return i
        return 0
    }

    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── Speichern-Leiste — fest am unteren Bildschirmrand (außerhalb von sheet) ──
    Rectangle {
        id: saveBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 64 * dp + root.safeBottom   // safeBottom = Höhe der Android-Navigationsleiste
        color: root.clCard
        z: 5

        Rectangle {
            width: parent.width; height: 1; color: root.clDivider
            anchors.top: parent.top
        }

        Row {
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                leftMargin: 16; rightMargin: 16; topMargin: 10
            }
            height: 52 * dp; spacing: 10

            // Abbrechen-Button
            Rectangle {
                width: (parent.width - 10) / 2; height: 52; radius: 10
                color: cancelTap.pressed ? "#C0C4C8" : "#DCE4E9"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Abbrechen")
                    color: "#1A1C1E"; font.pixelSize: 15 * dp; font.bold: true
                }
                MouseArea {
                    id: cancelTap; anchors.fill: parent
                    onClicked: settingsBridge.cancel()
                }
            }

            // Speichern-Button
            Rectangle {
                width: (parent.width - 10) / 2; height: 52; radius: 10
                color: saveTap.pressed ? "#004E7A" : root.clPrimary
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Speichern")
                    color: "white"; font.pixelSize: 15 * dp; font.bold: true
                }
                MouseArea {
                    id: saveTap; anchors.fill: parent
                    onClicked: {
                        // Warnung: Mehrere Fahrer aus, aber kein Fahrer angelegt
                        if (!mehrereFahrerSwitch.checked && settingsBridge.fahrerModel.length <= 1) {
                            warningDialog.open()
                            return
                        }
                        settingsBridge.save(
                            monatCb.currentId,
                            parseInt(jahrInput.text) || 2024,
                            mehrereFahrerSwitch.checked ? fahrerCb.currentId : 0,
                            adresseCb.currentId,
                            hinZurueckSwitch.checked,
                            mehrereFahrerSwitch.checked,
                            apiKeyInput.text,
                            syncModeCb.currentId,
                            parseInt(udpInput.portText) || 45455,
                            parseInt(tcpInput.portText) || 45454,
                            languageCb.currentId
                        )
                    }
                }
            }
        }
    }

    // ── Scrollbarer Inhalt — Tastatur schiebt nur diesen Bereich hoch ─────────
    Item {
        id: sheet
        anchors {
            top:    parent.top
            bottom: saveBar.top     // endet direkt über dem Speichern-Button
            left:   parent.left
            right:  parent.right
        }


    // ── Titelleiste ───────────────────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 40; color: root.clCard; z: 2

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1; color: root.clDivider
        }

        Text {
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: qsTr("Einstellungen")
            font.pixelSize: 14 * dp; font.bold: true; color: root.clPrimary
        }
    }

    // ── Scrollbares Formular ──────────────────────────────────────────────────
    ScrollView {
        id: sv
        anchors {
            top:    topBar.bottom
            bottom: parent.bottom
            left:   parent.left
            right:  parent.right
        }
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            width:         sv.availableWidth
            topPadding:    8
            bottomPadding: 24 + root.safeBottom
            spacing:       0

            // ── GRUPPE: Standardfilter ────────────────────────────────────────
            SectionHeader { text: qsTr("Standardfilter") }

            FieldLabel { text: qsTr("Monat") }
            FieldCombo {
                id: monatCb
                comboModel: settingsBridge.monatModel
                initId:     settingsBridge.initMonat
            }

            FieldSpacer {}
            FieldLabel { text: qsTr("Jahr") }

            Rectangle {
                x: 16; width: parent.width - 32; height: 48 * dp; radius: 8 * dp
                color: root.clCard
                border.color: jahrInput.activeFocus ? root.clPrimary : root.clOutline
                border.width: jahrInput.activeFocus ? 2 : 1

                TextInput {
                    id: jahrInput
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    text: settingsBridge.initJahr.toString()
                    font.pixelSize: 16 * dp; color: root.clOnSurf
                    inputMethodHints: Qt.ImhDigitsOnly
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    validator: IntValidator { bottom: 2000; top: 2099 }
                }
            }

            FieldSpacer {}

            // ── GRUPPE: Standard-Werte ────────────────────────────────────────
            // Reihenfolge: erst alle Schieberegler, dann alle Combos
            SectionHeader { text: qsTr("Standard-Werte") }

            // Schieberegler 1: Mehrere Fahrer
            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 12
                Switch {
                    id: mehrereFahrerSwitch
                    checked: settingsBridge.initMehrereFahrer
                }
                Text {
                    text: qsTr("Mehrere Fahrer")
                    font.pixelSize: 15 * dp; color: root.clOnSurf
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Schieberegler 2: Hin & Zurück
            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 12
                Switch {
                    id: hinZurueckSwitch
                    checked: settingsBridge.initHinZurueck
                }
                Text {
                    text: qsTr("Standard: Hin & Zurück")
                    font.pixelSize: 15 * dp; color: root.clOnSurf
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            FieldSpacer {}

            // Combo 1: Standard-Fahrer (nur sichtbar wenn Mehrere Fahrer aktiv)
            Column {
                width: parent.width
                spacing: 0
                visible: mehrereFahrerSwitch.checked

                FieldLabel { text: qsTr("Standard-Fahrer") }
                FieldCombo {
                    id: fahrerCb
                    comboModel: settingsBridge.fahrerModel
                    initId:     settingsBridge.initFahrerId
                    onComboModelChanged: currentIndex = root.findIdx(comboModel, initId)
                }
                FieldSpacer {}
            }

            // Combo 2: Standard-Adresse
            FieldLabel { text: qsTr("Standard-Adresse") }
            FieldCombo {
                id: adresseCb
                comboModel: settingsBridge.adressenModel
                initId:     settingsBridge.initAdresseId
                onComboModelChanged: currentIndex = root.findIdx(comboModel, initId)
            }

            FieldSpacer {}

            // ── GRUPPE: Distanzberechnung ─────────────────────────────────────
            SectionHeader { text: qsTr("Distanzberechnung") }

            Text {
                x: 16; width: parent.width - 32
                text: qsTr("Ohne Key: OSRM (kostenlos, kein Account). Mit Key: OpenRouteService (ORS, 500 Abfragen/Tag).")
                font.pixelSize: 12 * dp; color: root.clOutline
                wrapMode: Text.WordWrap
            }

            FieldSpacer {}

            FieldLabel { text: qsTr("ORS API-Key (optional)") }

            Rectangle {
                id: apiKeyRect
                x: 16; width: parent.width - 32; height: 48 * dp; radius: 8 * dp
                clip: true   // verhindert Textüberlauf über den Rand
                color: root.clCard
                border.color: apiKeyInput.activeFocus ? root.clPrimary : root.clOutline
                border.width: apiKeyInput.activeFocus ? 2 : 1

                TextInput {
                    id: apiKeyInput
                    // Rechter Rand: Platz für Auge-Button (36px) + je 8px Abstand
                    anchors { fill: parent; leftMargin: 12; rightMargin: 52 }
                    text: settingsBridge.initApiKey
                    font.pixelSize: 14 * dp; color: root.clOnSurf
                    echoMode: TextInput.Password
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText
                }

                // Auge-Toggle
                Rectangle {
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    width: 36; height: 36; radius: 4; color: "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: apiKeyInput.echoMode === TextInput.Password ? "👁" : "🔒"
                        font.pixelSize: 18 * dp
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: apiKeyInput.echoMode =
                            (apiKeyInput.echoMode === TextInput.Password)
                            ? TextInput.Normal : TextInput.Password
                    }
                }
            }

            FieldSpacer {}

            // ── GRUPPE: Sprache ───────────────────────────────────────────────
            SectionHeader { text: qsTr("Sprache") }

            FieldLabel { text: qsTr("App-Sprache") }
            FieldCombo {
                id: languageCb
                comboModel: settingsBridge.languageModel
                initId:     settingsBridge.initLanguage
            }
            Text {
                x: 16; width: parent.width - 32
                text: qsTr("Sprachänderung wird nach App-Neustart wirksam")
                font.pixelSize: 11 * dp; color: root.clOutline
                wrapMode: Text.WordWrap
                visible: languageCb.currentId !== settingsBridge.initLanguage
            }
            FieldSpacer {}

            // ── GRUPPE: Synchronisation ───────────────────────────────────────────────
            SectionHeader { text: qsTr("Synchronisation") }

            FieldLabel { text: qsTr("Modus") }
            FieldCombo {
                id: syncModeCb
                comboModel: settingsBridge.syncModeModel
                initId:     settingsBridge.initSyncMode
            }

            // WLAN-Ports
            Column {
                width: parent.width
                spacing: 0
                visible: syncModeCb.currentId === 1

                FieldSpacer {}
                SectionHeader { text: qsTr("WLAN-Ports (Standard empfohlen)") }

                FieldLabel { text: qsTr("UDP-Broadcast") }
                PortInput { id: udpInput; initVal: settingsBridge.initUdpPort }

                FieldSpacer {}
                FieldLabel { text: qsTr("TCP-Transfer") }
                PortInput { id: tcpInput; initVal: settingsBridge.initTcpPort }

                FieldSpacer {}
            }

            Item { width: 1; height: 8 }
        }
    }

    } // end sheet

    // ── Inline-Komponenten ────────────────────────────────────────────────────

    component SectionHeader: Rectangle {
        property alias text: sectionText.text
        width: parent.width
        height: sectionText.height + 16
        color: root.clSurface

        Text {
            id: sectionText
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 11 * dp; font.bold: true; color: root.clPrimary
        }

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right
                      leftMargin: 16; rightMargin: 16 }
            height: 1; color: root.clDivider
        }
    }

    component FieldLabel: Text {
        x: 16; width: parent.width - 32
        font.pixelSize: 12 * dp; color: root.clOutline
        topPadding: 6; bottomPadding: 4
    }

    component FieldSpacer: Item { width: 1; height: 14 }

    component PortInput: Item {
        x: 16; width: parent.width - 32; height: 48
        property int   initVal:  45454
        property alias portText: portTi.text

        Rectangle {
            anchors.fill: parent
            radius: 8; color: root.clCard
            border.color: portTi.activeFocus ? root.clPrimary : root.clOutline
            border.width: portTi.activeFocus ? 2 : 1

            TextInput {
                id: portTi
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                text: parent.parent.initVal.toString()
                font.pixelSize: 16 * dp; color: root.clOnSurf
                inputMethodHints: Qt.ImhDigitsOnly
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                validator: IntValidator { bottom: 1024; top: 65535 }
            }
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
            currentIndex = root.findIdx(comboModel, initId)
        }

        contentItem: Text {
            leftPadding: 12
            text: fc.displayText
            font: fc.font; color: root.clOnSurf
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8; color: root.clCard
            border.color: fc.pressed ? root.clPrimary : root.clOutline
            border.width: fc.pressed ? 2 : 1
        }

        popup: Popup {
            y: fc.height; width: fc.width
            implicitHeight: Math.min(contentItem.implicitHeight, 300)
            padding: 0

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: fc.delegateModel
                currentIndex: fc.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                radius: 8; color: root.clCard
                border.color: root.clOutline; border.width: 1
            }
        }

        delegate: ItemDelegate {
            width: fc.width; height: 48
            text: modelData ? modelData.text : ""
            font.pixelSize: 15 * dp
            highlighted: fc.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? "#E8F4FB" : root.clCard
            }
            contentItem: Text {
                text: parent.text; font: parent.font
                color: root.clOnSurf; leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // ── Warnung: Kein Fahrer angelegt ─────────────────────────────────────────
    Dialog {
        id: warningDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Kein Fahrer angelegt")

        Column {
            spacing: 12; width: parent ? parent.width : 300; padding: 8
            Text {
                width: parent.width - 16
                text: qsTr("Es ist kein Fahrer angelegt.\nBitte zuerst einen Fahrer anlegen,\nbevor \"Mehrere Fahrer\" deaktiviert wird.")
                wrapMode: Text.WordWrap
                font.pixelSize: 14 * root.dp
                color: root.clOnSurf
            }
        }

        standardButtons: Dialog.Ok
    }
}
