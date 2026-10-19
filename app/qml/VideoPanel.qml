import QtQuick
import Gcs 1.0
import "Theme.js" as T

// Video akışı + faydalı yük göstergeleri (artı imleci, gimbal açısı, kayıt)
Rectangle {
    color: "#000000"
    border.color: T.border

    VideoItem {
        id: videoItem
        anchors.fill: parent
        anchors.margins: 1
        source: Video
    }

    Text {
        anchors.centerIn: parent
        visible: !videoItem.hasFrame
        text: "Görüntü yok\n" + Video.status + "\n" + Video.uri
        horizontalAlignment: Text.AlignHCenter
        color: T.muted
        font.pixelSize: 13
    }

    // Artı imleci
    Item {
        anchors.centerIn: parent
        width: 40
        height: 40
        visible: videoItem.hasFrame
        Rectangle { anchors.centerIn: parent; width: 40; height: 1; color: "#80ffffff" }
        Rectangle { anchors.centerIn: parent; width: 1; height: 40; color: "#80ffffff" }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        color: "#a0000000"
        radius: 3
        width: info.implicitWidth + 12
        height: info.implicitHeight + 8
        Text {
            id: info
            anchors.centerIn: parent
            color: Video.streaming ? T.ok : T.warn
            font.family: T.mono
            font.pixelSize: 11
            text: (Video.streaming ? "CANLI " : "") + Video.frameWidth + "×" + Video.frameHeight +
                  " · " + Video.fps.toFixed(1) + " fps · düşen " + Video.framesDropped
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        visible: Payload.recording
        color: "#c0200000"
        radius: 3
        width: 62
        height: 22
        Row {
            anchors.centerIn: parent
            spacing: 5
            Rectangle {
                width: 10; height: 10; radius: 5; color: T.bad
                anchors.verticalCenter: parent.verticalCenter
            }
            Text { text: "KAYIT"; color: T.text; font.pixelSize: 11; font.bold: true }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        color: "#a0000000"
        radius: 3
        width: gim.implicitWidth + 12
        height: gim.implicitHeight + 8
        Text {
            id: gim
            anchors.centerIn: parent
            color: T.text
            font.family: T.mono
            font.pixelSize: 11
            text: "GİMBAL P " + Payload.gimbalPitch.toFixed(1) + "°  Y " + Payload.gimbalYaw.toFixed(1) +
                  "°  ZOOM %" + Payload.zoom.toFixed(0)
        }
    }
}
