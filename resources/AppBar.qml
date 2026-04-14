// AppBar.qml — MD3 Top App Bar (56px, im Layout)
// Kommunikation über "appBarBridge" (AppBarBridge).

import QtQuick 2.15
import QtQuick.Window 2.15

Item {
    id: root
    // dp: density-independent pixels
    // dp: Mindestwert 1.5 damit Schrift auf Tablets (niedrige DPI) lesbar bleibt
    // Qt skaliert auf Android intern bereits korrekt → dp = 1.0
    readonly property real dp: Math.min(Screen.width, Screen.height) >= 600 ? 1.25 : 1.0

    signal menuToggled()

    readonly property color clPrimary: "#006493"
    readonly property color clWhite:   "#FFFFFF"

    Rectangle {
        anchors.fill: parent
        color: root.clPrimary

        // Burger-Button
        Rectangle {
            id: burgerBtn
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            width: 48; height: 48 * dp; radius: 24
            color: burgerTap.pressed ? Qt.rgba(1,1,1,0.15) : "transparent"
            Column {
                anchors.centerIn: parent; spacing: 5
                Repeater {
                    model: 3
                    Rectangle { width: 22; height: 2.5; radius: 1; color: root.clWhite }
                }
            }
            MouseArea {
                id: burgerTap; anchors.fill: parent
                onClicked: root.menuToggled()
            }
        }

        // Seitentitel
        Text {
            id: titleText
            anchors { left: burgerBtn.right; leftMargin: 8; verticalCenter: parent.verticalCenter; right: rightCol.left; rightMargin: 8 }
            text: appBarBridge ? appBarBridge.pageTitle : qsTr("Driver's Log")
            color: root.clWhite; font.pixelSize: 18 * dp; font.bold: true
            elide: Text.ElideRight
        }

        // Versionsnummer + Sync-Status (rechts)
        Column {
            id: rightCol
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 1
            Text {
                id: versionText
                anchors.horizontalCenter: parent.horizontalCenter
                text: appBarBridge ? "v" + appBarBridge.appVersion.split(".").slice(0, appBarBridge.appVersion.split(".").length >= 4 && appBarBridge.appVersion.split(".")[3] !== "0" ? 4 : 3).join(".") : ""
                color: Qt.rgba(1,1,1,0.85); font.pixelSize: 11 * dp
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    onClicked: { if (appBarBridge) appBarBridge.onVersionTapped() }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: appBarBridge ? appBarBridge.syncStatus : ""
                color: Qt.rgba(1,1,1,0.75); font.pixelSize: 11 * dp
            }
        }
    }
}
