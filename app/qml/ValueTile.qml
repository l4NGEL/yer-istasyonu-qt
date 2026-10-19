import QtQuick
import "Theme.js" as T

// Etiket + değer + birim kutucuğu
Rectangle {
    id: tile
    property string label
    property string value: "—"
    property string unit
    property color valueColor: T.text
    implicitWidth: 100
    implicitHeight: 52
    radius: 4
    color: T.panel2
    border.color: T.border

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 2
        Text { text: tile.label; color: T.muted; font.pixelSize: 11 }
        Row {
            spacing: 4
            Text { text: tile.value; color: tile.valueColor; font.pixelSize: 18; font.family: T.mono }
            Text { text: tile.unit; color: T.muted; font.pixelSize: 11; anchors.baseline: parent.children[0].baseline }
        }
    }
}
