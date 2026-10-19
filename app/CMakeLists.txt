find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup()

# Uygulama ve Qt testlerinin paylaştığı kütüphane
add_library(gcs_qt STATIC
  src/link/LinkInterface.h
  src/link/UdpLink.h src/link/UdpLink.cpp
  src/vehicle/Vehicle.h src/vehicle/Vehicle.cpp
  src/mission/MissionModel.h src/mission/MissionModel.cpp
  src/mission/MissionManager.h src/mission/MissionManager.cpp
  src/payload/PayloadController.h src/payload/PayloadController.cpp
  src/video/PipelineBuilder.h src/video/PipelineBuilder.cpp
  src/video/VideoReceiver.h src/video/VideoReceiver.cpp
  src/video/VideoItem.h src/video/VideoItem.cpp
)
set_target_properties(gcs_qt PROPERTIES AUTOMOC ON)
target_include_directories(gcs_qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gcs_qt
  PUBLIC gcs_core Qt6::Core Qt6::Gui Qt6::Network Qt6::Quick
  PRIVATE gcs_warnings)

if(GCS_WITH_GSTREAMER)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
  target_link_libraries(gcs_qt PUBLIC PkgConfig::GST)
  target_compile_definitions(gcs_qt PUBLIC GCS_HAVE_GSTREAMER)
endif()

qt_add_executable(yer-istasyonu src/main.cpp)
target_link_libraries(yer-istasyonu PRIVATE gcs_qt Qt6::Qml Qt6::QuickControls2 gcs_warnings)

qt_add_resources(yer-istasyonu "qml"
  PREFIX "/"
  FILES
    qml/Main.qml
    qml/Theme.js
    qml/TopBar.qml
    qml/MapView.qml
    qml/VideoPanel.qml
    qml/AttitudeIndicator.qml
    qml/FlightPanel.qml
    qml/MissionPanel.qml
    qml/PayloadPanel.qml
    qml/ValueTile.qml
    qml/GcsSlider.qml
)
