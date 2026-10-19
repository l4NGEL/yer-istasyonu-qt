import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Gcs 1.0
import "Theme.js" as T

ColumnLayout {
    spacing: 10

    component Labeled: ColumnLayout {
        property alias title: titleText.text
        property alias detail: detailText.text
        spacing: 2
        Layout.fillWidth: true
        RowLayout {
            Text { id: titleText; color: T.muted; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Text { id: detailText; color: T.text; font.pixelSize: 11; font.family: T.mono }
        }
    }

    Labeled {
        title: "Gimbal yunuslama (pitch)"
        detail: "hedef " + Payload.targetPitch.toFixed(0) + "° · ölçülen " +
                (Payload.gimbalReporting ? Payload.gimbalPitch.toFixed(1) + "°" : "—")
        GcsSlider {
            id: pitchSlider
            Layout.fillWidth: true
            from: -90; to: 30; stepSize: 1
            value: Payload.targetPitch
            onMoved: Payload.setGimbal(value, yawSlider.value)
        }
    }
    Labeled {
        title: "Gimbal sapma (yaw, gövdeye göre)"
        detail: "hedef " + Payload.targetYaw.toFixed(0) + "° · ölçülen " +
                (Payload.gimbalReporting ? Payload.gimbalYaw.toFixed(1) + "°" : "—")
        GcsSlider {
            id: yawSlider
            Layout.fillWidth: true
            from: -180; to: 180; stepSize: 1
            value: Payload.targetYaw
            onMoved: Payload.setGimbal(pitchSlider.value, value)
        }
    }
    RowLayout {
        spacing: 6
        Button { text: "Ortala"; enabled: Vehicle.connected; onClicked: Payload.centerGimbal() }
        Button { text: "Aşağı bak"; enabled: Vehicle.connected; onClicked: Payload.lookDown() }
    }

    Labeled {
        title: "Yakınlaştırma"
        detail: "%" + zoomSlider.value.toFixed(0)
        GcsSlider {
            id: zoomSlider
            Layout.fillWidth: true
            from: 0; to: 100; stepSize: 1
            value: Payload.zoom
            // Sürükleme bitince tek komut gönderilir
            onPressedChanged: if (!pressed) Payload.setZoom(value)
        }
    }

    RowLayout {
        spacing: 6
        Button {
            text: "Fotoğraf çek"
            enabled: Vehicle.connected
            implicitHeight: 36
            onClicked: Payload.takePhoto()
        }
        Button {
            text: Payload.recording ? "Kaydı durdur" : "Kayıt başlat"
            enabled: Vehicle.connected
            implicitHeight: 36
            palette.button: Payload.recording ? T.bad : T.panel2
            onClicked: Payload.toggleRecording()
        }
        Item { Layout.fillWidth: true }
        ValueTile { label: "Fotoğraf"; value: Payload.photoCount.toString() }
    }

    Text {
        Layout.fillWidth: true
        text: Payload.lastResult.length ? "Son komut: " + Payload.lastResult : ""
        color: T.muted
        wrapMode: Text.WordWrap
        font.pixelSize: 12
    }

    Text {
        Layout.fillWidth: true
        text: "Gimbal: MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW (10 Hz birleştirilmiş). " +
              "Ölçülen açı GIMBAL_DEVICE_ATTITUDE_STATUS kuaterniyonundan çözülür. " +
              "Kamera: IMAGE_START_CAPTURE, VIDEO_START/STOP_CAPTURE, SET_CAMERA_ZOOM."
        color: T.muted
        wrapMode: Text.WordWrap
        font.pixelSize: 11
    }

    Item { Layout.fillHeight: true }
}
