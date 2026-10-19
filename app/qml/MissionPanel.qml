import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Gcs 1.0
import "Theme.js" as T

ColumnLayout {
    spacing: 8

    component SmallButton: Button {
        implicitHeight: 30
        font.pixelSize: 12
    }

    RowLayout {
        spacing: 6
        ValueTile { label: "Öğe"; value: Mission.count.toString() }
        ValueTile {
            label: "Yol uzunluğu"
            value: Mission.totalDistance >= 1000 ? (Mission.totalDistance / 1000).toFixed(2)
                                                 : Mission.totalDistance.toFixed(0)
            unit: Mission.totalDistance >= 1000 ? "km" : "m"
        }
        ColumnLayout {
            spacing: 2
            Text { text: "Yeni nokta irtifası (m)"; color: T.muted; font.pixelSize: 11 }
            SpinBox {
                from: 5; to: 500; stepSize: 5
                value: Mission.defaultAltitude
                editable: true
                implicitHeight: 30
                onValueModified: Mission.defaultAltitude = value
            }
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: 6
        SmallButton { text: "Kalkış ekle"; onClicked: Mission.appendTakeoff(Mission.defaultAltitude) }
        SmallButton { text: "Eve dönüş ekle"; onClicked: Mission.appendReturnToLaunch() }
        SmallButton { text: "Temizle"; onClicked: Mission.clear() }
        SmallButton { text: "Kaydet…"; onClicked: saveDialog.open() }
        SmallButton { text: "Aç…"; onClicked: openDialog.open() }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Button {
            text: MissionManager.busy ? "İptal" : "Araca yükle"
            implicitHeight: 32
            enabled: Vehicle.connected && (MissionManager.busy || Mission.count > 0)
            highlighted: !MissionManager.busy
            onClicked: MissionManager.busy ? MissionManager.cancel() : MissionManager.upload()
        }
        ProgressBar {
            Layout.fillWidth: true
            value: MissionManager.progress
        }
    }
    Text {
        Layout.fillWidth: true
        visible: text.length > 0
        text: (MissionManager.lastResult.length ? "Yükleme: " + MissionManager.lastResult : "") +
              (MissionManager.retransmissions > 0 ? "  (yeniden gönderim " + MissionManager.retransmissions + ")" : "") +
              (Mission.lastError.length ? "\nDosya: " + Mission.lastError : "")
        color: Mission.lastError.length || (!MissionManager.lastSucceeded && !MissionManager.busy) ? T.warn : T.ok
        wrapMode: Text.WordWrap
        font.pixelSize: 12
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: T.panel2
        border.color: T.border
        radius: 3

        Text {
            anchors.centerIn: parent
            visible: Mission.count === 0
            text: "Haritaya tıklayarak yol noktası ekleyin"
            color: T.muted
            font.pixelSize: 12
        }

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            spacing: 2
            model: Mission
            delegate: Rectangle {
                id: row
                required property int index
                required property string commandName
                required property double altitude
                required property double holdTime
                required property bool hasPosition
                required property double latitude
                required property double longitude
                width: ListView.view.width
                height: 40
                radius: 3
                color: Vehicle.connected && Vehicle.missionCurrent === index ? "#2a2410" : T.panel

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    spacing: 6
                    Text {
                        text: (row.index + 1) + "."
                        color: T.path; font.bold: true; font.pixelSize: 13
                        Layout.preferredWidth: 24
                    }
                    Column {
                        Layout.fillWidth: true
                        Text { text: row.commandName; color: T.text; font.pixelSize: 12 }
                        Text {
                            visible: row.hasPosition
                            text: row.latitude.toFixed(6) + ", " + row.longitude.toFixed(6)
                            color: T.muted; font.pixelSize: 10; font.family: T.mono
                        }
                    }
                    SpinBox {
                        from: 0; to: 500; stepSize: 5
                        value: row.altitude
                        editable: true
                        implicitWidth: 104
                        implicitHeight: 30
                        visible: row.commandName !== "Eve dönüş"
                        onValueModified: Mission.setAltitude(row.index, value)
                    }
                    ToolButton { text: "▲"; enabled: row.index > 0; onClicked: Mission.move(row.index, row.index - 1) }
                    ToolButton {
                        text: "▼"
                        enabled: row.index < Mission.count - 1
                        onClicked: Mission.move(row.index, row.index + 1)
                    }
                    ToolButton { text: "✕"; onClicked: Mission.remove(row.index) }
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: "Görevi kaydet"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Görev planı (*.plan)", "Tüm dosyalar (*)"]
        defaultSuffix: "plan"
        onAccepted: Mission.save(selectedFile.toString())
    }
    FileDialog {
        id: openDialog
        title: "Görev aç"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Görev planı (*.plan)", "Tüm dosyalar (*)"]
        onAccepted: Mission.load(selectedFile.toString())
    }
}
