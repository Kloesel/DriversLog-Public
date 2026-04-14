// DatePickerDialog.qml
// Nur QtQuick 2.15 + QtQuick.Controls 2.15 + QtQuick.Handlers (in 2.15 enthalten).
// TapHandler statt MouseArea – auf Android/QQuickWidget werden Touch-Events
// von TapHandler zuverlaessiger verarbeitet als von MouseArea.
//
// Schnittstelle zu C++:
//   Context-Property  initialIsoDate  (String "YYYY-MM-DD")
//   Signal            dateAccepted(string isoDate)
//   Signal            cancelled()

import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    // dp: density-independent pixels
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    property string initialIsoDate: ""
    signal dateAccepted(string isoDate)
    signal cancelled()

    property int selYear:  0
    property int selMonth: 0
    property int selDay:   0
    property int navYear:  0
    property int navMonth: 0

    Component.onCompleted: {
        var d = new Date()
        if (initialIsoDate.length === 10) {
            var p = initialIsoDate.split("-")
            var c = new Date(parseInt(p[0]), parseInt(p[1])-1, parseInt(p[2]))
            if (!isNaN(c.getTime())) d = c
        }
        selYear  = d.getFullYear()
        selMonth = d.getMonth() + 1
        selDay   = d.getDate()
        navYear  = selYear
        navMonth = selMonth
        rebuildGrid()
    }

    function daysInMonth(y, m)  { return new Date(y, m, 0).getDate() }
    function firstWeekday(y, m) { return (new Date(y, m-1, 1).getDay() + 6) % 7 }
    function toIso(y, m, d)     { return y+"-"+(m<10?"0":"")+m+"-"+(d<10?"0":"")+d }
    function monthName(m) {
        return [qsTr("Januar"),qsTr("Februar"),qsTr("März"),qsTr("April"),
                qsTr("Mai"),qsTr("Juni"),qsTr("Juli"),qsTr("August"),
                qsTr("September"),qsTr("Oktober"),qsTr("November"),qsTr("Dezember")][m-1]||""
    }

    function rebuildGrid() {
        dayModel.clear()
        var offset = firstWeekday(navYear, navMonth)
        var dimCur = daysInMonth(navYear, navMonth)
        var prevM  = navMonth===1 ? 12 : navMonth-1
        var prevY  = navMonth===1 ? navYear-1 : navYear
        var dimPrev= daysInMonth(prevY, prevM)
        for (var i = offset-1; i >= 0; i--)
            dayModel.append({day: dimPrev-i, month: prevM, year: prevY, cur: false})
        for (var dd = 1; dd <= dimCur; dd++)
            dayModel.append({day: dd, month: navMonth, year: navYear, cur: true})
        var nextM = navMonth===12 ? 1 : navMonth+1
        var nextY = navMonth===12 ? navYear+1 : navYear
        for (var n = 1; n <= 42-dayModel.count; n++)
            dayModel.append({day: n, month: nextM, year: nextY, cur: false})
    }

    function stepMonth(delta) {
        var d = new Date(navYear, navMonth-1+delta, 1)
        navYear  = d.getFullYear()
        navMonth = d.getMonth()+1
        rebuildGrid()
    }

    // ════════════════════════════════════════════════════════════════
    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 12
        border.color: "#DCE4E9"; border.width: 1

        Column {
            anchors { fill: parent; margins: 14 }
            spacing: 8

            // ── Navigation ───────────────────────────────────────
            Row {
                width: parent.width; height: 48

                // ◄ Vormonat
                Rectangle {
                    id: prevBtn
                    width: 48; height: 48 * dp; radius: 24
                    color: prevTap.pressed ? "#DCE4E9" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "<"; font.pixelSize: 22 * dp; font.bold: true; color: "#006493"
                    }
                    TapHandler {
                        id: prevTap
                        onTapped: root.stepMonth(-1)
                    }
                }

                Text {
                    width: parent.width - 96
                    height: 48
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.monthName(navMonth) + "  " + navYear
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 17; font.bold: true; color: "#1A1C1E"
                }

                // ► Nächster Monat
                Rectangle {
                    id: nextBtn
                    width: 48; height: 48 * dp; radius: 24
                    color: nextTap.pressed ? "#DCE4E9" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: ">"; font.pixelSize: 22 * dp; font.bold: true; color: "#006493"
                    }
                    TapHandler {
                        id: nextTap
                        onTapped: root.stepMonth(+1)
                    }
                }
            }

            // ── Wochentag-Header ─────────────────────────────────
            Row {
                width: parent.width
                Repeater {
                    model: ["Mo","Di","Mi","Do","Fr","Sa","So"]
                    Text {
                        width:  parent.width / 7
                        height: 28 * dp
                        text:   modelData
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:   Text.AlignVCenter
                        font.pixelSize: 11 * dp; font.bold: true; color: "#72787E"
                    }
                }
            }

            // ── Tage-Grid ────────────────────────────────────────
            ListModel { id: dayModel }

            GridView {
                id: grid
                width:      parent.width
                height:     cellHeight * 6
                cellWidth:  Math.floor(width / 7)
                cellHeight: 44
                interactive: false
                model: dayModel

                delegate: Rectangle {
                    id: cell
                    width:  grid.cellWidth
                    height: grid.cellHeight
                    color: "transparent"

                    property bool isSel:
                        model.day   === root.selDay   &&
                        model.month === root.selMonth &&
                        model.year  === root.selYear

                    Rectangle {
                        anchors.centerIn: parent
                        width: 40; height: 40; radius: 20
                        color: cell.isSel ? "#006493"
                               : dayTap.pressed ? "#E8F0FE" : "transparent"
                    }
                    Text {
                        anchors.centerIn: parent
                        text:  model.day
                        font.pixelSize: 15 * dp
                        font.bold:      cell.isSel
                        color: !model.cur  ? "#C0C8CE"
                               : cell.isSel ? "#FFFFFF"
                                            : "#1A1C1E"
                    }
                    TapHandler {
                        id: dayTap
                        onTapped: {
                            root.selDay   = model.day
                            root.selMonth = model.month
                            root.selYear  = model.year
                            if (!model.cur) {
                                root.navMonth = model.month
                                root.navYear  = model.year
                                root.rebuildGrid()
                            }
                        }
                    }
                }
            }

            // ── OK / Abbrechen ───────────────────────────────────
            Row {
                width: parent.width
                spacing: 10
                layoutDirection: Qt.RightToLeft
                topPadding: 6

                Rectangle {
                    width:  (parent.width - 10) / 2
                    height: 52
                    radius: 10
                    color:  okTap.pressed ? "#004E7A" : "#006493"
                    Text {
                        anchors.centerIn: parent
                        text: "OK"; color: "#FFF"
                        font.pixelSize: 16 * dp; font.bold: true
                    }
                    TapHandler {
                        id: okTap
                        onTapped: root.dateAccepted(
                            root.toIso(root.selYear, root.selMonth, root.selDay))
                    }
                }

                Rectangle {
                    width:  (parent.width - 10) / 2
                    height: 52
                    radius: 10
                    color:  canTap.pressed ? "#C4CDD4" : "#DCE4E9"
                    Text {
                        anchors.centerIn: parent
                        text: "Abbrechen"; color: "#1A1C1E"
                        font.pixelSize: 14 * dp
                    }
                    TapHandler {
                        id: canTap
                        onTapped: root.cancelled()
                    }
                }
            }
        }
    }
}
