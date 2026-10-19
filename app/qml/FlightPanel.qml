import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Gcs 1.0
import "Theme.js" as T

ColumnLayout {
    spacing: 8

    component Cmd: Button {
        Layout.fillWidth: true
        implicitHeight: 32
        enabled: Vehicle.connected
        font.pixelSize: 12
    }

    RowLayout {
        spacing: 10
        AttitudeIndicator {
            Layout.preferredWidth: 160
            Layout.preferredHeight: 160
            roll: Vehicle.roll
            pitch: Vehicle.pitch
            heading: Vehicle.heading
        }
        GridLayout {
            columns: 3
            rowSpacing: 6
            columnSpacing: 6
            ValueTile { label: "İrtifa (bağıl)"; value: Vehicle.relativeAltitude.toFixed(1); unit: "m" }
            ValueTile { label: "Yer hızı"; value: Vehicle.groundSpeed.toFixed(1); unit: "m/s" }
            ValueTile { label: "Tırmanma"; value: Vehicle.climbRate.toFixed(1); unit: "m/s" }
            ValueTile { label: "Yön"; value: Math.round(Vehicle.heading).toString(); unit: "°" }
            ValueTile { label: "Gaz"; value: Vehicle.throttle.toString(); unit: "%" }
            ValueTile {
                label: "Görev öğesi"
                value: Vehicle.missionTotal > 0
                       ? Math.min(Vehicle.missionCurrent + 1, Vehicle.missionTotal) + "/" + Vehicle.missionTotal
                       : "—"
            }
        }
    }

    Text {
        text: Vehicle.positionValid
              ? Vehicle.latitude.toFixed(6) + ", " + Vehicle.longitude.toFixed(6) + "   MSL " +
                Vehicle.altitudeMsl.toFixed(1) + " m"
              : "Konum yok"
        color: T.muted
        font.family: T.mono
        font.pixelSize: 12
    }

    GridLayout {
        columns: 3
        rowSpacing: 6
        columnSpacing: 6
        Layout.fillWidth: true
        Cmd { text: Vehicle.armed ? "Disarm" : "Arm"; onClicked: Vehicle.armed ? Vehicle.disarm() : Vehicle.arm() }
        Cmd { text: "Görevi başlat"; onClicked: Vehicle.startMission() }
        Cmd { text: "Duraklat"; onClicked: Vehicle.pauseMission() }
        Cmd { text: "Devam"; onClicked: Vehicle.resumeMission() }
        Cmd { text: "Eve dön"; onClicked: Vehicle.returnToLaunch() }
        Cmd { text: "İniş"; onClicked: Vehicle.land() }
    }

    Text { text: "Olay günlüğü"; color: T.muted; font.pixelSize: 11 }
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: T.panel2
        border.color: T.border
        radius: 3
        ListView {
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            model: Vehicle.statusLog
            delegate: Text {
                required property string modelData
                width: ListView.view.width
                text: modelData
                color: T.text
                elide: Text.ElideRight
                font.family: T.mono
                font.pixelSize: 11
            }
        }
    }
}
