import QtQuick
import QtQuick.Layouts
import Gcs 1.0
import "Theme.js" as T

Rectangle {
    color: T.panel
    border.color: T.border
    implicitHeight: 48

    component Badge: Rectangle {
        property alias text: label.text
        property color tint: T.muted
        radius: 3
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.16)
        border.color: tint
        implicitWidth: label.implicitWidth + 16
        implicitHeight: 24
        Text { id: label; anchors.centerIn: parent; color: parent.tint; font.pixelSize: 12; font.bold: true }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        Text { text: "YER İSTASYONU"; color: T.text; font.pixelSize: 15; font.bold: true; font.letterSpacing: 1.5 }

        Rectangle {
            width: 10; height: 10; radius: 5
            color: Vehicle.connected ? T.ok : T.bad
        }
        Text {
            text: Vehicle.connected ? "Bağlı · sistem " + Vehicle.systemId : "Araç yok"
            color: T.text; font.pixelSize: 13
        }
        Badge { text: Vehicle.modeName; tint: T.accent; visible: Vehicle.connected }
        Badge {
            text: Vehicle.armed ? "ARMED" : "DISARMED"
            tint: Vehicle.armed ? T.bad : T.ok
            visible: Vehicle.connected
        }

        Item { Layout.fillWidth: true }

        Text {
            color: Vehicle.batteryRemaining >= 0 && Vehicle.batteryRemaining < 25 ? T.warn : T.text
            font.family: T.mono; font.pixelSize: 13
            text: "BAT " + Vehicle.batteryVoltage.toFixed(1) + " V  " +
                  (Vehicle.batteryRemaining >= 0 ? Vehicle.batteryRemaining + "%" : "—")
        }
        Text {
            color: Vehicle.gpsFixType >= 3 ? T.text : T.warn
            font.family: T.mono; font.pixelSize: 13
            text: "GPS " + (Vehicle.gpsFixType >= 3 ? "3D" : Vehicle.gpsFixType === 2 ? "2D" : "YOK") +
                  " · " + Vehicle.satellites + " uydu"
        }
        Text {
            color: Link.lossPercent > 5 ? T.warn : T.muted
            font.family: T.mono; font.pixelSize: 12
            text: "UDP :" + Link.localPort + " ← " + (Link.remote.length ? Link.remote : "—") +
                  "  " + Link.framesReceived + " çerçeve · kayıp %" + Link.lossPercent.toFixed(1) +
                  " · CRC " + Link.crcErrors
        }
    }
}
