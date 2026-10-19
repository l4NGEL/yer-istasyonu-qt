import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Koyu temaya uygun kaydırıcı: koyu ray, vurgu renginde dolu kısım.
// "from" negatifse dolgu sıfırdan başlar (gimbal açısı gibi işaretli değerler).
Slider {
    id: control
    implicitHeight: 28

    readonly property real zeroPos: from < 0 && to > 0 ? (0 - from) / (to - from) : 0

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: T.border
        Rectangle {
            readonly property real a: Math.min(control.zeroPos, control.visualPosition)
            readonly property real b: Math.max(control.zeroPos, control.visualPosition)
            x: a * parent.width
            width: (b - a) * parent.width
            height: parent.height
            radius: 2
            color: T.accent
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: control.pressed ? T.text : T.accent
        border.color: T.bg
        border.width: 2
    }
}
