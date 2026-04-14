// FahrtFormDialog.qml
// Vollbild-Formular fuer Neue Fahrt / Fahrt bearbeiten auf Android.
// Kommunikation ausschliesslich ueber das "bridge"-Objekt (FahrtFormBridge),
// das von C++ via QQmlContext::setContextProperty gesetzt wird.
// Benoetigt: QtQuick 2.15, QtQuick.Controls 2.15

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Item {
    id: root

    // Bridge-Guard: verhindert TypeError wenn bridge noch nicht gesetzt ist
    property bool hasBridge: bridge != null

    // ── Farben (passend zu md3_style.qss) ────────────────────────────────────
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

    // Speichern-Button aktiv wenn alle Pflichtfelder gesetzt sind
    readonly property bool saveOk: root.hasBridge
        && startCb.currentId !== 0
        && zielCb.currentId !== 0

    readonly property int safeAreaBottom: Math.max(0,
        Screen.height - Screen.desktopAvailableHeight)
    readonly property real kbHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height / Screen.devicePixelRatio
        : 0

    property double oneWayKm: (bridge && bridge.initHz && bridge.initEntf > 0)
                               ? bridge.initEntf / 2.0 : (bridge ? bridge.initEntf : 0)

    // ISO-Datum (YYYY-MM-DD) → deutsches Format (TT.MM.JJJJ)
    function isoToDE(iso) {
        if (!iso || iso.length !== 10) return ""
        var p = iso.split("-")
        return p[2] + "." + p[1] + "." + p[0]
    }

    // Findet Index im Array anhand der id
    function findIdx(model, id) {
        for (var i = 0; i < model.length; i++)
            if (model[i].id === id) return i
        return 0
    }

    // ── Reaktion auf Bridge-Signale ───────────────────────────────────────────
    Connections {
        target: root.hasBridge ? bridge : null

        function onOneWayKmChanged() {
            root.oneWayKm = bridge.oneWayKm
            kmInput.text  = (hzSwitch.checked
                             ? bridge.oneWayKm * 2
                             : bridge.oneWayKm).toFixed(1)
        }

        function onDistanceStatus(text, calculating) {
            distStatus.text   = text
            calcBtn.available = !calculating
        }

        // Nach Anlegen eines neuen Fahrers: savedId der ComboBox setzen
        function onNewFahrerCreated(id) {
            if (fahrerLoader.item) {
                var row = fahrerLoader.item.children[1]   // Column > Row
                if (row) {
                    var combo = row.children[0]            // Row > FieldCombo
                    if (combo) combo.savedId = id
                }
            }
        }

        // Nach Anlegen einer neuen Adresse: savedId der richtigen ComboBox setzen
        // damit onComboModelChanged die neue Adresse automatisch auswählt.
        function onNewAdressCreated(id, forField) {
            if (forField === 1) startCb.savedId = id
            if (forField === 2) zielCb.savedId  = id
        }
    }

    // Scroll-Timer: wartet bis Tastatur offen ist, scrollt dann zum bemRect
    Timer {
        id: scrollTimer
        interval: 350
        repeat: false
        onTriggered: {
            if (!bemRect.visible) return
            var pos    = bemRect.mapToItem(sv.contentItem, 0, 0)
            var bottom = pos.y + bemRect.height + 24
            var need   = bottom - sv.height
            if (need > sv.contentItem.contentY)
                sv.contentItem.contentY = Math.max(0, need)
        }
    }

    // ── Hintergrund ───────────────────────────────────────────────────────────
    Rectangle { anchors.fill: parent; color: root.clSurface }

    // ── Gesamter Inhalt – bei Tastatur nach oben verschieben ──────────────────
    Item {
        id: sheet
        anchors.fill: parent


    // ── Kopfzeile ─────────────────────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 56 * dp; color: root.clPrimary; z: 2
        Text {
            anchors.centerIn: parent
            text:  root.hasBridge ? (bridge.isNew ? qsTr("Neue Fahrt") : qsTr("Fahrt bearbeiten")) : ""
            color: root.clWhite; font.pixelSize: 18 * dp; font.bold: true
        }
    }

    // ── Scrollbares Formular ──────────────────────────────────────────────────
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

            // ── Datum ──────────────────────────────────────────────────────────
            FieldLabel { text: qsTr("Datum *") }

            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 8

                Rectangle {
                    width: parent.width - 56
                    height: 48 * dp; radius: 8 * dp; color: root.clWhite
                    border.color: root.clOutline; border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text:  root.hasBridge ? root.isoToDE(bridge.datumIso) : ""
                        font.pixelSize: 16 * dp; color: root.clOnSurf
                    }
                    TapHandler { onTapped: bridge.openCalendar() }
                }

                Rectangle {
                    width: 48; height: 48 * dp; radius: 8 * dp
                    color: calTap.pressed ? "#004E7A" : root.clPrimary

                    Item {
                        anchors.centerIn: parent
                        width: 26; height: 24

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: root.clWhite; border.width: 2
                            radius: 3
                        }
                        Rectangle {
                            anchors { top: parent.top; left: parent.left; right: parent.right }
                            height: 7; radius: 2
                            color: root.clWhite; opacity: 0.9
                        }
                        Rectangle { x: 5;  y: -2; width: 3; height: 5; radius: 1; color: root.clWhite }
                        Rectangle { x: 18; y: -2; width: 3; height: 5; radius: 1; color: root.clWhite }
                        Repeater {
                            model: [{cx:4,cy:13},{cx:11,cy:13},{cx:18,cy:13},
                                    {cx:4,cy:19},{cx:11,cy:19},{cx:18,cy:19}]
                            Rectangle {
                                x: modelData.cx; y: modelData.cy
                                width: 3; height: 3; radius: 1
                                color: root.clWhite; opacity: 0.85
                            }
                        }
                    }

                    TapHandler { id: calTap; onTapped: bridge.openCalendar() }
                }
            }

            FieldSpacer {}

            // ── Fahrer (nur wenn mehrerefahrer) ───────────────────────────────
            Loader {
                id: fahrerLoader
                width: parent.width
                active: root.hasBridge && bridge.mehrerefahrer
                sourceComponent: Column {
                    width: parent ? parent.width : 0; spacing: 0
                    FieldLabel { text: qsTr("Fahrer") }
                    Row {
                        x: 16; width: parent.width - 32
                        height: 52 * dp; spacing: 8
                        FieldCombo {
                            id: fahrerCb
                            width: parent.width - 60
                            x: 0
                            comboModel: bridge.fahrerModel
                            initId:     bridge.initFahrerId
                        }
                        // + Neuer Fahrer
                        Rectangle {
                            width: 52; height: 52; radius: 8
                            color: addFahrerTap.pressed ? "#004E7A" : root.clPrimary
                            Rectangle {
                                anchors.centerIn: parent
                                width: 20; height: 3; radius: 1; color: "white"
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 3; height: 20; radius: 1; color: "white"
                            }
                            TapHandler {
                                id: addFahrerTap
                                onTapped: bridge.requestNewFahrer()
                            }
                        }
                    }
                    FieldSpacer {}
                }
            }

            // ── Startadresse ──────────────────────────────────────────────────
            FieldLabel { text: qsTr("Startadresse *") }

            // Kombi aus FieldCombo + + -Button
            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 8

                FieldCombo {
                    id: startCb
                    width: parent.width - 60
                    x: 0
                    comboModel: root.hasBridge ? bridge.adressenModel : []
                    initId:     root.hasBridge ? bridge.initStartId : 0
                }

                // + -Button: neue Adresse anlegen (nur bei neuer Fahrt)
                Rectangle {
                    width: 52; height: 52; radius: 8
                    color: addStartTap.pressed ? "#004E7A" : root.clPrimary

                    Rectangle {
                        anchors.centerIn: parent
                        width: 20; height: 3; radius: 1; color: "white"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 3; height: 20; radius: 1; color: "white"
                    }

                    TapHandler {
                        id: addStartTap
                        onTapped: bridge.requestNewAdress(1)
                    }
                }
            }

            FieldSpacer {}

            // ── Zieladresse ───────────────────────────────────────────────────
            FieldLabel { text: qsTr("Zieladresse *") }

            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 8

                FieldCombo {
                    id: zielCb
                    width: parent.width - 60
                    x: 0
                    comboModel: root.hasBridge ? bridge.adressenModel : []
                    initId:     root.hasBridge ? bridge.initZielId : 0
                }

                // + -Button: neue Adresse anlegen (nur bei neuer Fahrt)
                Rectangle {
                    width: 52; height: 52; radius: 8
                    color: addZielTap.pressed ? "#004E7A" : root.clPrimary

                    Rectangle {
                        anchors.centerIn: parent
                        width: 20; height: 3; radius: 1; color: "white"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 3; height: 20; radius: 1; color: "white"
                    }

                    TapHandler {
                        id: addZielTap
                        onTapped: bridge.requestNewAdress(2)
                    }
                }
            }

            FieldSpacer {}

            // ── Entfernung ────────────────────────────────────────────────────
            FieldLabel { text: qsTr("Entfernung (km)") }

            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 8

                Rectangle {
                    width: parent.width - 130; height: 48 * dp; radius: 8 * dp
                    color: root.clWhite
                    border.color: root.clOutline; border.width: 1
                    TextInput {
                        id: kmInput
                        anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                        text: root.hasBridge ? bridge.initEntf.toFixed(1) : "0.0"
                        font.pixelSize: 16 * dp; color: root.clOnSurf
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        verticalAlignment: TextInput.AlignVCenter
                    }
                }

                Rectangle {
                    id: calcBtn
                    property bool available: true
                    width: 120; height: 48 * dp; radius: 8 * dp
                    color: calcTap.pressed ? "#004E7A"
                           : available ? root.clPrimary : "#A0C4D8"
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Berechnen"); color: root.clWhite; font.pixelSize: 13 * dp
                    }
                    TapHandler {
                        id: calcTap
                        enabled: calcBtn.available
                        onTapped: bridge.calcDistance(startCb.currentId, zielCb.currentId)
                    }
                }
            }

            Text {
                id: distStatus
                x: 16; width: parent.width - 32
                text: ""; color: root.clOutline; font.pixelSize: 12 * dp
                wrapMode: Text.WordWrap
            }

            FieldSpacer {}

            // ── Hin & Zurueck ─────────────────────────────────────────────────
            Row {
                x: 16; width: parent.width - 32; height: 52 * dp; spacing: 12
                Switch {
                    id: hzSwitch
                    checked: root.hasBridge && bridge.initHz
                    onCheckedChanged: {
                        if (root.oneWayKm > 0)
                            kmInput.text = (checked
                                ? root.oneWayKm * 2
                                : root.oneWayKm).toFixed(1)
                    }
                }
                Text {
                    text: qsTr("Hin & Zurück")
                    font.pixelSize: 15 * dp; color: root.clOnSurf
                }
            }

            FieldSpacer {}

            // ── Bemerkung ─────────────────────────────────────────────────────
            FieldLabel { text: qsTr("Bemerkung") }

            Rectangle {
                id: bemRect
                x: 16; width: parent.width - 32; height: 48 * dp; radius: 8 * dp
                color: root.clWhite
                border.color: bemInput.activeFocus ? root.clPrimary : root.clOutline
                border.width: bemInput.activeFocus ? 2 : 1
                TextInput {
                    id: bemInput
                    anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                    text: root.hasBridge ? bridge.initBem : ""
                    font.pixelSize: 16 * dp; color: root.clOnSurf
                    verticalAlignment: TextInput.AlignVCenter
                    onActiveFocusChanged: {
                        if (activeFocus) scrollTimer.restart()
                    }
                }
            }

            Item { width: 1; height: 8 }

            Text {
                x: 16; text: qsTr("* Pflichtfelder")
                color: root.clOutline; font.pixelSize: 12 * dp
            }

            Item { width: 1; height: 16 }
        }
    }

    // ── Button-Leiste ─────────────────────────────────────────────────────────
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
                left:        parent.left
                right:       parent.right
                top:         parent.top
                topMargin:   10
                leftMargin:  10
                rightMargin: 10
            }
            height: 52
            spacing: 8

            Rectangle {
                visible: root.hasBridge && !bridge.isNew
                width:   visible ? (parent.width - 16) / 3 : 0
                height: 52; radius: 10
                color: delTap.pressed ? "#8B0000" : root.clDanger
                Text {
                    anchors.centerIn: parent; text: qsTr("Löschen")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true
                }
                TapHandler { id: delTap; onTapped: bridge.deleteFahrt() }
            }

            Rectangle {
                width:  root.hasBridge && bridge.isNew
                        ? (parent.width - 8)  / 2
                        : (parent.width - 16) / 3
                height: 52; radius: 10
                color: canTap.pressed ? "#C4CDD4" : "#DCE4E9"
                Text {
                    anchors.centerIn: parent; text: qsTr("Abbrechen")
                    color: root.clOnSurf; font.pixelSize: 14 * dp
                }
                TapHandler { id: canTap; onTapped: { Qt.inputMethod.hide(); bridge.cancel() } }
            }

            Rectangle {
                width:  root.hasBridge && bridge.isNew
                        ? (parent.width - 8)  / 2
                        : (parent.width - 16) / 3
                height: 52; radius: 10
                color: !root.saveOk        ? root.clOutline
                     : saveTap.pressed     ? "#004E7A"
                     :                       root.clPrimary
                Text {
                    anchors.centerIn: parent; text: qsTr("Speichern")
                    color: root.clWhite; font.pixelSize: 14 * dp; font.bold: true
                }
                TapHandler {
                    id: saveTap
                    enabled: root.saveOk
                    onTapped: {
                        Qt.inputMethod.hide()
                        var fid = 0
                        if (bridge.mehrerefahrer) {
                            var litem = fahrerLoader.item
                            if (litem) {
                                // Column > Row (children[1]) > FieldCombo (children[0])
                                var row = litem.children[1]
                                if (row) {
                                    var combo = row.children[0]
                                    if (combo) fid = combo.currentId
                                }
                            }
                        }
                        bridge.save(
                            bridge.datumIso,
                            fid,
                            startCb.currentId,
                            zielCb.currentId,
                            parseFloat(kmInput.text.replace(",", ".")) || 0.0,
                            hzSwitch.checked,
                            bemInput.text.trim()
                        )
                    }
                }
            }
        }
    }

    } // end sheet

    // ── Wiederverwendbare Inline-Komponenten ──────────────────────────────────

    component FieldLabel: Text {
        x: 16; width: parent.width - 32
        font.pixelSize: 12 * dp; color: root.clOutline
        topPadding: 4; bottomPadding: 4
    }

    component FieldSpacer: Item { width: 1; height: 14 }

    component FieldCombo: ComboBox {
        id: fc
        x: 16; width: parent.width - 32; height: 52
        property var comboModel: []
        property int initId:     0
        property int currentId:  currentIndex >= 0 && currentIndex < comboModel.length
                                  ? comboModel[currentIndex].id : 0
        // savedId: überlebt Modell-Neuladungen (z.B. nach Anlegen neuer Adresse)
        property int savedId: 0

        model:     comboModel
        textRole:  "text"
        font.pixelSize: 15 * dp

        Component.onCompleted: {
            savedId      = initId
            currentIndex = root.findIdx(comboModel, initId)
        }

        // Neue Adresse ausgewählt → savedId merken
        onCurrentIndexChanged: {
            if (currentIndex >= 0 && currentIndex < comboModel.length)
                savedId = comboModel[currentIndex].id
        }

        // Modell neu geladen (z.B. nach + -Button) → Auswahl per savedId wiederherstellen
        onComboModelChanged: {
            var idx = root.findIdx(comboModel, savedId)
            if (currentIndex !== idx)
                currentIndex = idx
        }

        contentItem: Text {
            leftPadding: 12
            text: fc.displayText
            font: fc.font; color: root.clOnSurf
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: root.clWhite
            border.color: fc.pressed ? root.clPrimary : root.clOutline
            border.width: fc.pressed ? 2 : 1
        }

        popup: Popup {
            y:      fc.height
            width:  fc.width
            implicitHeight: Math.min(contentItem.implicitHeight, 300)
            padding: 0

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: fc.delegateModel
                currentIndex: fc.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle { radius: 8; color: root.clWhite
                border.color: root.clOutline; border.width: 1 }
        }

        delegate: ItemDelegate {
            width: fc.width
            height: 48
            text: modelData ? modelData.text : ""
            font.pixelSize: 15 * dp
            highlighted: fc.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? "#E8F4FB" : root.clWhite
            }
            contentItem: Text {
                text: parent.text; font: parent.font
                color: root.clOnSurf; leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
