// SyncLogView.qml — MD3 Sync-Protokoll für Android

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
    readonly property color clDanger  : "#BA1A1A"
    readonly property color clWarn    : "#e67e22"
    readonly property color clSuccess : "#27ae60"

    readonly property int  safeBottom: 0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    // 0=log, 1=matrix  (gesteuert durch filterCombo.currentIndex == 4)
    property bool showMatrix: false

    Rectangle { anchors.fill: parent; color: root.clSurface }

    // showHeader: true = Android (kein Fensterrahmen), false = Windows (Rahmen hat X)
    property bool showHeader: true

    // ── App-Bar ────────────────────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.showHeader ? 56 * dp : 0
        visible: root.showHeader
        color: root.clPrimary; z: 2

        Text {
            anchors.centerIn: parent
            text: root.showMatrix ? qsTr("Knowledge Matrix") : qsTr("Sync-Protokoll")
            color: "white"; font.pixelSize: 18 * dp; font.bold: true
        }
    }

    // ── Filter-Leiste ──────────────────────────────────────────────────────
    Rectangle {
        id: filterBar
        anchors { top: topBar.bottom; left: parent.left; right: parent.right }
        height: 52; color: root.clCard; z: 1

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1; color: root.clDivider
        }

        Row {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            spacing: 6

            Repeater {
                model: logBridge.filterModel
                Rectangle {
                    height: 30 * dp; width: chipLabel.width + 20; radius: 15
                    color: filterCombo.currentIndex === index
                           ? root.clPrimary : root.clDivider
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: chipLabel
                        anchors.centerIn: parent
                        text: modelData.text
                        font.pixelSize: 12 * dp
                        color: filterCombo.currentIndex === index
                               ? "white" : root.clOnSurf
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            filterCombo.currentIndex = index
                            root.showMatrix = (index === 4)
                            logBridge.applyFilter(index)
                        }
                    }
                }
            }

            ComboBox {
                id: filterCombo
                visible: false
                model: logBridge.filterModel.length
            }

            // 30-Tage-Bereinigen (nur im Log-Modus sinnvoll)
            Rectangle {
                height: 30 * dp; width: pruneLabel.width + 20; radius: 15
                visible: !root.showMatrix
                color: pruneTap.pressed ? root.clDanger : "#DCE4E9"
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    id: pruneLabel
                    anchors.centerIn: parent
                    text: qsTr("30 Tage")
                    font.pixelSize: 12 * dp; color: root.clOnSurf
                }
                MouseArea {
                    id: pruneTap
                    anchors.fill: parent
                    onClicked: logBridge.pruneAndReload()
                }
            }
        }
    }

    // ── Zusammenfassung ────────────────────────────────────────────────────
    Rectangle {
        id: summaryBar
        anchors { top: filterBar.bottom; left: parent.left; right: parent.right }
        height: 30 * dp; color: "#EEF4FB"

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1; color: root.clDivider
        }

        Text {
            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
            text: root.showMatrix ? logBridge.matrixSummary : logBridge.summary
            font.pixelSize: 11 * dp; color: "#1F4E79"; font.bold: true
            elide: Text.ElideRight
            width: parent.width - 16
        }
    }

    // ── Änderungslog-Liste ─────────────────────────────────────────────────
    ListView {
        id: logList
        anchors { top: summaryBar.bottom; bottom: btnBar.top; left: parent.left; right: parent.right }
        visible: !root.showMatrix
        clip: true; spacing: 0
        model: logBridge.entries

        // Scroll-Position nach Daten-Reload erhalten
        Connections {
            target: logBridge
            function onEntriesChanged() {
                var savedY = logList.contentY
                logList.model = logBridge.entries  // force refresh
                logList.contentY = savedY
            }
        }

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        header: Rectangle {
            width: logList.width; height: 32; color: "#DCE4E9"
            Row {
                anchors { fill: parent; leftMargin: 12 }
                spacing: 0
                HeaderCell { text: qsTr("Zeit");    colWidth: 82  }
                HeaderCell { text: qsTr("Tabelle"); colWidth: 68  }
                HeaderCell { text: qsTr("Aktion");  colWidth: 66  }
                HeaderCell { text: qsTr("Seq");     colWidth: 44  }
                HeaderCell { text: qsTr("Richtg."); colWidth: 54  }
                HeaderCell { text: qsTr("Status");  colWidth: 60  }
                HeaderCell { text: qsTr("Gerät");   colWidth: -1  }
            }
        }

        delegate: Item {
            width: logList.width
            height: rowRect.height + 1

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.clDivider
            }

            Rectangle {
                id: rowRect
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 40; color: index % 2 === 0 ? root.clCard : "#F4F6F7"

                Row {
                    anchors { fill: parent; leftMargin: 12 }
                    spacing: 0
                    BodyCell { text: modelData.ts;      colWidth: 82;  textColor: root.clSub    }
                    BodyCell { text: modelData.table;   colWidth: 68;  textColor: root.clOnSurf }
                    BodyCell { text: modelData.op;      colWidth: 66;  textColor: modelData.opColor }
                    BodyCell {
                        text: modelData.seq;  colWidth: 44
                        textColor: modelData.seq === "–" ? "#ccc" : "#1F4E79"
                        bold: true
                    }
                    BodyCell { text: modelData.dir;     colWidth: 54;  textColor: modelData.dirColor }
                    BodyCell { text: modelData.status;  colWidth: 60;  textColor: modelData.statusColor }
                    BodyCell { text: modelData.device;  colWidth: -1;  textColor: root.clSub }
                }
            }
        }

        Item {
            anchors.centerIn: parent
            visible: logList.count === 0
            Text {
                anchors.centerIn: parent
                text: qsTr("Keine Einträge")
                font.pixelSize: 16 * dp; color: root.clSub
            }
        }
    }

    // ── Knowledge Matrix ───────────────────────────────────────────────────
    Item {
        id: matrixView
        anchors { top: summaryBar.bottom; bottom: btnBar.top; left: parent.left; right: parent.right }
        visible: root.showMatrix

        // ── Sortier-Leiste ─────────────────────────────────────────────────
        Rectangle {
            id: matrixSortBar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 40; color: root.clCard
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.clDivider
            }
            Row {
                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Sortieren:")
                    font.pixelSize: 11 * dp; color: root.clSub
                }

                Repeater {
                    model: [
                        { key: "origin", label: qsTr("Quelle")  },
                        { key: "peer",   label: qsTr("Ziel")    },
                        { key: "seq",    label: qsTr("Seq ↓")   }
                    ]
                    Rectangle {
                        height: 26 * dp; width: sortChipLbl.width + 16; radius: 13
                        anchors.verticalCenter: parent.verticalCenter
                        color: matrixList.sortKey === modelData.key
                               ? root.clPrimary : root.clDivider

                        Text {
                            id: sortChipLbl
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: 11 * dp
                            color: matrixList.sortKey === modelData.key
                                   ? "white" : root.clOnSurf
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (matrixList.sortKey === modelData.key) {
                                    matrixList.sortAsc = !matrixList.sortAsc
                                } else {
                                    matrixList.sortKey = modelData.key
                                    matrixList.sortAsc = (modelData.key !== "seq")
                                }
                                matrixList.rebuildSorted()
                            }
                        }
                    }
                }
            }
        }

        // ── Legende ────────────────────────────────────────────────────────
        Rectangle {
            id: matrixHeader
            anchors { top: matrixSortBar.bottom; left: parent.left; right: parent.right }
            height: 22; color: "#F0F4F8"
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.clDivider
            }
            Row {
                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                spacing: 16
                Text { text: qsTr("Q = Quelle");    font.pixelSize: 10 * dp; color: root.clSub }
                Text { text: qsTr("Z = Ziel"); font.pixelSize: 10 * dp; color: root.clSub }
                Text { text: qsTr("\u24c1 = lokal"); font.pixelSize: 10 * dp; color: "#1F4E79" }
            }
        }

        ListView {
            id: matrixList
            anchors {
                top: matrixHeader.bottom
                bottom: parent.bottom
                left: parent.left; right: parent.right
            }
            clip: true
            model: sortedArray

            // Sortierstatus
            property string sortKey: "origin"
            property bool   sortAsc: true
            property var    sortedArray: []   // JS-Array als Modell: eine Zuweisung statt clear()+append()

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            function rebuildSorted() {
                var src = logBridge.knowledgeRows
                var arr = []
                for (var i = 0; i < src.length; i++) arr.push(src[i])
                var k = matrixList.sortKey
                var asc = matrixList.sortAsc
                arr.sort(function(a, b) {
                    var va = k === "seq" ? a.seq : (k === "origin" ? a.origin : a.peer)
                    var vb = k === "seq" ? b.seq : (k === "origin" ? b.origin : b.peer)
                    if (typeof va === "string") va = va.toLowerCase()
                    if (typeof vb === "string") vb = vb.toLowerCase()
                    if (va < vb) return asc ? -1 :  1
                    if (va > vb) return asc ?  1 : -1
                    return 0
                })
                var savedY = matrixList.contentY
                matrixList.sortedArray = arr
                matrixList.contentY = savedY
            }

            delegate: Item {
                id: delegateRoot
                width: matrixList.width
                height: matCard.height + 1

                Rectangle {
                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                    height: 1; color: root.clDivider
                }

                // ── Karte ──────────────────────────────────────────────────
                Rectangle {
                    id: matCard
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: cardContent.height + 16
                    color: modelData.isSelf  ? "#EEF4FB"
                           : index % 2 === 0 ? root.clCard : "#EDF7F0"

                    // Linke Spalte: Peer + Ursprung gestapelt
                    // Rechte Spalte: Seq-Badge + Ausst-Badge
                    Row {
                        id: cardContent
                        anchors {
                            left: parent.left; right: parent.right
                            leftMargin: 12; rightMargin: 12
                            top: parent.top; topMargin: 8
                        }
                        spacing: 8

                        // ── Links: Labels ────────────────────────────────────
                        Column {
                            width: parent.width - badgeCol.width - 8
                            spacing: 2

                            // Quelle-Zeile (Ursprung) – zuerst
                            Row {
                                spacing: 4
                                Text {
                                    text: qsTr("Q:")
                                    font.pixelSize: 10 * dp
                                    color: root.clSub
                                    anchors.baseline: originText.baseline
                                }
                                Text {
                                    id: originText
                                    width: parent.parent.width - 16
                                    text: modelData.origin
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 13 * dp
                                    font.bold: modelData.isSelf
                                    color: modelData.isSelf ? "#1F4E79" : root.clOnSurf
                                    lineHeight: 1.25
                                }
                            }

                            // Ziel-Zeile (Peer)
                            Row {
                                spacing: 4
                                Text {
                                    text: qsTr("Z:")
                                    font.pixelSize: 10 * dp
                                    color: root.clSub
                                    anchors.baseline: peerText.baseline
                                }
                                Text {
                                    id: peerText
                                    width: parent.parent.width - 16
                                    text: modelData.peer
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 12 * dp
                                    color: modelData.isSelf ? "#4A7FAF" : root.clSub
                                    lineHeight: 1.25
                                }
                            }
                        }

                        // ── Rechts: Badges ───────────────────────────────────
                        Column {
                            id: badgeCol
                            width: 72 * dp
                            spacing: 4
                            anchors.top: parent.top

                            // Seq-Badge: "Peer/Max"
                            Rectangle {
                                width: parent.width; height: 34 * dp
                                radius: 6
                                color: modelData.isSelf ? "#D0E8F8"
                                     : modelData.seqColor === "#27ae60" ? "#E8F8EE"
                                     : modelData.seqColor === "#e67e22" ? "#FFF3E0"
                                     : "#F5F5F5"

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Q/Z")
                                        font.pixelSize: 9 * dp
                                        color: modelData.isSelf ? "#1F4E79" : root.clSub
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.seqText
                                        font.pixelSize: 12 * dp; font.bold: true
                                        color: modelData.isSelf ? "#1F4E79" : modelData.seqColor
                                    }
                                }
                            }

                            // Status-Badge: ✓ oder "N fehlen"
                            Rectangle {
                                width: parent.width; height: 28 * dp
                                radius: 6
                                color: modelData.isSelf             ? "#EEF4FB"
                                     : modelData.statusColor === "#27ae60" ? "#E8F8EE"
                                     : modelData.statusColor === "#e67e22" ? "#FFF3E0"
                                     : "#F5F5F5"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.statusText
                                    font.pixelSize: 11 * dp
                                    font.bold: modelData.statusColor === "#27ae60"
                                    color: modelData.statusColor
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }
                }
            }

            Item {
                anchors.centerIn: parent
                visible: matrixList.count === 0
                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Noch keine Sync-Partner bekannt")
                        font.pixelSize: 16 * dp; color: root.clSub
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Die Matrix füllt sich nach dem ersten WLAN-Sync")
                        font.pixelSize: 12 * dp; color: root.clSub
                    }
                }
            }
        }

        // JS-Array als Modell: eine Zuweisung statt clear()+append() → kein Layout-Reset

        // Bei neuen Daten neu sortieren, Scroll-Position erhalten
        Connections {
            target: logBridge
            function onKnowledgeChanged() {
                matrixList.rebuildSorted()
            }
        }
    }  // Item matrixView ──────────────────────────────────────────────────────
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 64 * dp + root.safeBottom; color: root.clCard

        Rectangle {
            width: parent.width; height: 1; color: root.clDivider
            anchors.top: parent.top
        }

        Rectangle {
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                leftMargin: 16; rightMargin: 16; topMargin: 10
            }
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

    // ── Inline-Komponenten ─────────────────────────────────────────────────
    component HeaderCell: Item {
        property string text:     ""
        property int    colWidth: 80
        height: parent.height
        width:  colWidth < 0 ? (parent.width - parent.x) : colWidth

        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: parent.text
            font.pixelSize: 11 * dp; font.bold: true; color: root.clOnSurf
            elide: Text.ElideRight
            width: parent.width - 4
        }
    }

    component BodyCell: Item {
        property string text:      ""
        property int    colWidth:  80
        property color  textColor: root.clOnSurf
        property bool   bold:      false
        height: parent.height
        width:  colWidth < 0 ? (parent.width - parent.x) : colWidth

        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: parent.text
            font.pixelSize: 12 * dp; font.bold: parent.bold
            color: parent.textColor
            elide: Text.ElideRight
            width: parent.width - 4
        }
    }
}
