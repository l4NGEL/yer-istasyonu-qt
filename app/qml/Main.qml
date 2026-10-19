import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Gcs 1.0
import "Theme.js" as T

ApplicationWindow {
    id: window
    width: 1600
    height: 900
    visible: true
    title: "Yer İstasyonu"
    color: T.bg

    // main.cpp'deki --screenshot-tabs bu özelliği değiştirir
    property int currentTab: 0

    palette.window: T.bg
    palette.windowText: T.text
    palette.base: T.panel2
    palette.text: T.text
    palette.button: T.panel2
    palette.buttonText: T.text
    palette.highlight: T.accent
    palette.highlightedText: "#001020"
    palette.mid: T.border
    palette.dark: T.border
    palette.light: T.panel2

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar { Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            MapView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.preferredWidth: 540
                Layout.fillHeight: true
                color: T.panel
                border.color: T.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    VideoPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 9 / 16
                    }

                    TabBar {
                        id: tabs
                        Layout.fillWidth: true
                        currentIndex: window.currentTab
                        onCurrentIndexChanged: window.currentTab = currentIndex
                        TabButton { text: "Uçuş" }
                        TabButton { text: "Görev (" + Mission.count + ")" }
                        TabButton { text: "Faydalı yük" }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: tabs.currentIndex
                        FlightPanel {}
                        MissionPanel {}
                        PayloadPanel {}
                    }
                }
            }
        }
    }
}
