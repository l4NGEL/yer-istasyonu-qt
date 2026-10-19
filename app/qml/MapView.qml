import QtQuick
import QtQuick.Controls
import Gcs 1.0
import "Theme.js" as T

// Çevrimdışı ızgara harita: yerel eşdikdörtgen izdüşüm (birkaç km'lik görev
// alanında hata ihmal edilebilir). Karo sunucusu gerektirmez; arka plan
// harita karoları bu sürümde yoktur.
//
// Planlama kipinde: sol tık yol noktası ekler, nokta sürüklenir, sağ tık siler.
// Her kipte: tekerlek yakınlaştırır, boş alanda sürükleme kaydırır.
Item {
    id: map
    clip: true

    property real centerLat: 39.8890
    property real centerLon: 32.7785
    property real metersPerPixel: 0.5
    property bool planMode: true
    property bool followVehicle: false
    property bool centeredOnce: false
    property var trail: []
    property real cursorLat: NaN
    property real cursorLon: NaN

    readonly property real mPerDegLat: 111195.0
    readonly property real mPerDegLon: 111195.0 * Math.cos(centerLat * Math.PI / 180)

    function toX(lon) { return width / 2 + (lon - centerLon) * mPerDegLon / metersPerPixel }
    function toY(lat) { return height / 2 - (lat - centerLat) * mPerDegLat / metersPerPixel }
    function toLon(x) { return centerLon + (x - width / 2) * metersPerPixel / mPerDegLon }
    function toLat(y) { return centerLat - (y - height / 2) * metersPerPixel / mPerDegLat }

    function zoomAt(px, py, factor) {
        const lat = toLat(py), lon = toLon(px)
        metersPerPixel = Math.max(0.05, Math.min(50, metersPerPixel * factor))
        // İmlecin altındaki nokta yerinde kalsın
        centerLat += lat - toLat(py)
        centerLon += lon - toLon(px)
    }
    // Görev noktalarını (ve varsa aracı) kenar boşluğuyla ekrana sığdırır.
    function fitMission() {
        const pts = Mission.pathPoints()
        if (Vehicle.positionValid) pts.push({ lat: Vehicle.latitude, lon: Vehicle.longitude })
        if (pts.length === 0 || width <= 0 || height <= 0) return false
        let minLat = pts[0].lat, maxLat = pts[0].lat, minLon = pts[0].lon, maxLon = pts[0].lon
        for (const p of pts) {
            minLat = Math.min(minLat, p.lat); maxLat = Math.max(maxLat, p.lat)
            minLon = Math.min(minLon, p.lon); maxLon = Math.max(maxLon, p.lon)
        }
        centerLat = (minLat + maxLat) / 2
        centerLon = (minLon + maxLon) / 2
        const spanN = (maxLat - minLat) * mPerDegLat, spanE = (maxLon - minLon) * mPerDegLon
        metersPerPixel = Math.max(0.2, Math.min(50, Math.max(spanN / (height * 0.75), spanE / (width * 0.75))))
        return true
    }
    function centerOnVehicle() {
        if (Vehicle.positionValid) {
            centerLat = Vehicle.latitude
            centerLon = Vehicle.longitude
        }
    }
    function niceStep(minMeters) {
        const steps = [5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000, 10000]
        for (let i = 0; i < steps.length; ++i) if (steps[i] >= minMeters) return steps[i]
        return 20000
    }
    function repaintAll() { grid.requestPaint(); overlay.requestPaint() }

    Component.onCompleted: Qt.callLater(fitMission)
    onCenterLatChanged: repaintAll()
    onCenterLonChanged: repaintAll()
    onMetersPerPixelChanged: repaintAll()
    onWidthChanged: repaintAll()
    onHeightChanged: repaintAll()

    Connections {
        target: Vehicle
        function onPositionChanged() {
            if (!map.centeredOnce) {
                map.centeredOnce = true
                if (Mission.count === 0 || !map.fitMission()) map.centerOnVehicle()
            }
            const t = map.trail
            const last = t.length ? t[t.length - 1] : null
            if (!last || Math.abs(last.lat - Vehicle.latitude) > 2e-6 || Math.abs(last.lon - Vehicle.longitude) > 2e-6) {
                t.push({ lat: Vehicle.latitude, lon: Vehicle.longitude })
                if (t.length > 3000) t.shift()
            }
            if (map.followVehicle) map.centerOnVehicle()
            else overlay.requestPaint()
        }
        function onHomeChanged() { overlay.requestPaint() }
    }
    Connections {
        target: Mission
        function onGeometryChanged() { overlay.requestPaint() }
        function onCountChanged() { overlay.requestPaint() }
    }

    Rectangle { anchors.fill: parent; color: T.mapBg }

    // Metre ızgarası; ana çizgiler 5 adımda bir
    Canvas {
        id: grid
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const step = map.niceStep(map.metersPerPixel * 60)
            const stepLat = step / map.mPerDegLat, stepLon = step / map.mPerDegLon
            const lat0 = Math.floor(map.toLat(height) / stepLat) * stepLat
            const lon0 = Math.floor(map.toLon(0) / stepLon) * stepLon
            ctx.lineWidth = 1
            ctx.font = "10px 'DejaVu Sans Mono'"
            for (let lat = lat0; lat < map.toLat(0) + stepLat; lat += stepLat) {
                const y = Math.round(map.toY(lat)) + 0.5
                const major = Math.round(lat / stepLat) % 5 === 0
                ctx.strokeStyle = major ? T.gridMajor : T.grid
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                if (major) { ctx.fillStyle = T.gridMajor; ctx.fillText(lat.toFixed(5) + "°", 4, y - 3) }
            }
            for (let lon = lon0; lon < map.toLon(width) + stepLon; lon += stepLon) {
                const x = Math.round(map.toX(lon)) + 0.5
                const major = Math.round(lon / stepLon) % 5 === 0
                ctx.strokeStyle = major ? T.gridMajor : T.grid
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                if (major) { ctx.fillStyle = T.gridMajor; ctx.fillText(lon.toFixed(5) + "°", x + 3, height - 6) }
            }
        }
    }

    // Görev yolu, iz ve ev konumu
    Canvas {
        id: overlay
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            // Uçulan iz
            const t = map.trail
            if (t.length > 1) {
                ctx.strokeStyle = T.trail
                ctx.globalAlpha = 0.7
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(map.toX(t[0].lon), map.toY(t[0].lat))
                for (let i = 1; i < t.length; ++i) ctx.lineTo(map.toX(t[i].lon), map.toY(t[i].lat))
                ctx.stroke()
                ctx.globalAlpha = 1
            }
            // Görev yolu (konumlu öğeler arasında); evden ilk noktaya kesikli
            const pts = Mission.pathPoints().map(p => ({ x: map.toX(p.lon), y: map.toY(p.lat) }))
            ctx.lineWidth = 2
            ctx.strokeStyle = T.path
            if (Vehicle.homeValid && pts.length) {
                ctx.setLineDash([6, 6])
                ctx.beginPath()
                ctx.moveTo(map.toX(Vehicle.homeLongitude), map.toY(Vehicle.homeLatitude))
                ctx.lineTo(pts[0].x, pts[0].y)
                ctx.stroke()
                ctx.setLineDash([])
            }
            if (pts.length > 1) {
                ctx.beginPath()
                ctx.moveTo(pts[0].x, pts[0].y)
                for (let k = 1; k < pts.length; ++k) ctx.lineTo(pts[k].x, pts[k].y)
                ctx.stroke()
            }
            if (Vehicle.homeValid) {
                const hx = map.toX(Vehicle.homeLongitude), hy = map.toY(Vehicle.homeLatitude)
                ctx.fillStyle = T.home
                ctx.beginPath(); ctx.arc(hx, hy, 9, 0, 2 * Math.PI); ctx.fill()
                ctx.fillStyle = "#1a1030"
                ctx.font = "bold 11px 'DejaVu Sans'"
                ctx.fillText("H", hx - 4, hy + 4)
            }
        }
    }

    // Boş alan: kaydırma, yakınlaştırma, yol noktası ekleme
    MouseArea {
        id: panArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        property real pressX
        property real pressY
        property real startLat
        property real startLon
        property bool dragged
        onPressed: (m) => {
            pressX = m.x; pressY = m.y; startLat = map.centerLat; startLon = map.centerLon; dragged = false
        }
        onPositionChanged: (m) => {
            map.cursorLat = map.toLat(m.y)
            map.cursorLon = map.toLon(m.x)
            if (!pressed) return
            if (Math.abs(m.x - pressX) + Math.abs(m.y - pressY) > 4) dragged = true
            if (dragged) {
                map.followVehicle = false
                map.centerLat = startLat + (m.y - pressY) * map.metersPerPixel / map.mPerDegLat
                map.centerLon = startLon - (m.x - pressX) * map.metersPerPixel / map.mPerDegLon
            }
        }
        onReleased: (m) => {
            if (!dragged && map.planMode) Mission.appendWaypoint(map.toLat(m.y), map.toLon(m.x))
        }
        onWheel: (w) => map.zoomAt(w.x, w.y, w.angleDelta.y > 0 ? 0.8 : 1.25)
    }

    // Yol noktaları
    Repeater {
        id: missionRepeater
        model: Mission
        delegate: Item {
            id: wp
            required property int index
            required property double latitude
            required property double longitude
            required property double altitude
            required property bool hasPosition
            readonly property bool current: Vehicle.connected && Vehicle.missionCurrent === index
            visible: hasPosition
            x: map.toX(longitude) - 13
            y: map.toY(latitude) - 13
            width: 26
            height: 26
            z: 5

            Rectangle {
                anchors.fill: parent
                radius: 13
                color: wp.current ? T.path : "#2a2410"
                border.color: T.path
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: wp.index + 1
                    color: wp.current ? "#1a1400" : T.path
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            Text {
                x: 28; y: -12
                text: wp.altitude.toFixed(0) + " m"
                color: T.path
                font.pixelSize: 10
                font.family: T.mono
                style: Text.Outline; styleColor: "#000000"
            }
            MouseArea {
                anchors.fill: parent
                enabled: map.planMode
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true
                cursorShape: Qt.SizeAllCursor
                onClicked: (m) => { if (m.button === Qt.RightButton) Mission.remove(wp.index) }
                onPositionChanged: (m) => {
                    if (!(m.buttons & Qt.LeftButton)) return
                    const p = mapToItem(map, m.x, m.y)
                    Mission.setCoordinate(wp.index, map.toLat(p.y), map.toLon(p.x))
                }
            }
        }
    }

    // Araç simgesi
    Item {
        id: vehicleMarker
        visible: Vehicle.positionValid
        x: map.toX(Vehicle.longitude) - width / 2
        y: map.toY(Vehicle.latitude) - height / 2
        width: 34
        height: 34
        z: 10
        rotation: Vehicle.heading
        Canvas {
            id: vehicleCanvas
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = Vehicle.armed ? T.bad : T.accent
                ctx.strokeStyle = "#000000"
                ctx.lineWidth = 1.5
                ctx.beginPath()
                ctx.moveTo(17, 2); ctx.lineTo(30, 30); ctx.lineTo(17, 23); ctx.lineTo(4, 30)
                ctx.closePath(); ctx.fill(); ctx.stroke()
            }
            Connections { target: Vehicle; function onStatusChanged() { vehicleCanvas.requestPaint() } }
        }
    }

    // Ölçek çubuğu ve imleç koordinatı
    Column {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        anchors.bottomMargin: 24
        spacing: 4
        Rectangle {
            readonly property real meters: map.niceStep(map.metersPerPixel * 100)
            width: meters / map.metersPerPixel
            height: 4
            color: T.text
            Text {
                anchors.bottom: parent.top
                anchors.bottomMargin: 2
                text: parent.meters >= 1000 ? (parent.meters / 1000) + " km" : parent.meters + " m"
                color: T.text; font.pixelSize: 11; font.family: T.mono
            }
        }
        Text {
            text: isNaN(map.cursorLat) ? "" : map.cursorLat.toFixed(6) + ", " + map.cursorLon.toFixed(6)
            color: T.muted; font.pixelSize: 11; font.family: T.mono
        }
    }

    // Harita denetimleri
    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 6
        z: 20
        component MapButton: Button {
            implicitWidth: 120
            implicitHeight: 30
            font.pixelSize: 12
            palette.button: checked ? T.accent : T.panel2
            palette.buttonText: checked ? "#001020" : T.text
        }
        MapButton { text: "Planlama"; checkable: true; checked: map.planMode; onToggled: map.planMode = checked }
        MapButton { text: "Aracı takip et"; checkable: true; checked: map.followVehicle
                    onToggled: { map.followVehicle = checked; if (checked) map.centerOnVehicle() } }
        MapButton { text: "Araca ortala"; onClicked: map.centerOnVehicle() }
        MapButton { text: "Görevi sığdır"; onClicked: { map.followVehicle = false; map.fitMission() } }
        MapButton { text: "Yakınlaş  +"; onClicked: map.zoomAt(map.width / 2, map.height / 2, 0.7) }
        MapButton { text: "Uzaklaş  −"; onClicked: map.zoomAt(map.width / 2, map.height / 2, 1.4) }
        MapButton { text: "İzi temizle"; onClicked: { map.trail = []; overlay.requestPaint() } }
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 12
        text: map.planMode ? "Planlama: tık = nokta ekle · sürükle = taşı · sağ tık = sil"
                           : "Görüntüleme kipi"
        color: T.muted
        font.pixelSize: 12
    }
}
