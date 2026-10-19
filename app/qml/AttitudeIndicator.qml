import QtQuick
import "Theme.js" as T

// Yapay ufuk: yuvarlanma (roll) ufku döndürür, yunuslama (pitch) kaydırır.
Item {
    id: adi
    property real roll: 0      // derece
    property real pitch: 0     // derece
    property real heading: 0   // derece
    readonly property real pxPerDeg: height / 60

    onRollChanged: canvas.requestPaint()
    onPitchChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            const w = width, h = height, r = Math.min(w, h) / 2
            ctx.reset()
            ctx.save()
            ctx.beginPath(); ctx.arc(w / 2, h / 2, r - 1, 0, 2 * Math.PI); ctx.clip()
            ctx.translate(w / 2, h / 2)
            ctx.rotate(-adi.roll * Math.PI / 180)
            ctx.translate(0, adi.pitch * adi.pxPerDeg)
            ctx.fillStyle = "#2b6cb0"; ctx.fillRect(-2 * w, -2 * h, 4 * w, 2 * h)
            ctx.fillStyle = "#7a4e22"; ctx.fillRect(-2 * w, 0, 4 * w, 2 * h)
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2
            ctx.beginPath(); ctx.moveTo(-2 * w, 0); ctx.lineTo(2 * w, 0); ctx.stroke()
            ctx.lineWidth = 1
            ctx.fillStyle = "#ffffff"
            ctx.font = "9px sans-serif"
            for (let p = -30; p <= 30; p += 10) {
                if (p === 0) continue
                const y = -p * adi.pxPerDeg
                const half = (p % 20 === 0) ? 26 : 16
                ctx.beginPath(); ctx.moveTo(-half, y); ctx.lineTo(half, y); ctx.stroke()
                ctx.fillText(Math.abs(p), half + 4, y + 3)
            }
            ctx.restore()
            // Sabit uçak simgesi
            ctx.strokeStyle = T.path
            ctx.lineWidth = 3
            ctx.beginPath()
            ctx.moveTo(w / 2 - 40, h / 2); ctx.lineTo(w / 2 - 12, h / 2); ctx.lineTo(w / 2, h / 2 + 8)
            ctx.lineTo(w / 2 + 12, h / 2); ctx.lineTo(w / 2 + 40, h / 2)
            ctx.stroke()
            ctx.strokeStyle = T.border
            ctx.lineWidth = 2
            ctx.beginPath(); ctx.arc(w / 2, h / 2, r - 1, 0, 2 * Math.PI); ctx.stroke()
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 6
        text: Math.round(adi.heading) + "°"
        color: T.text
        font.family: T.mono
        font.pixelSize: 12
        style: Text.Outline
        styleColor: "#000000"
    }
}
