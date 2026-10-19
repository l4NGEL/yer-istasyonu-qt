# Qt'ye bağımlı olmayan çekirdek: MAVLink kodlayıcı, görev protokolü, komut takibi,
# coğrafi hesaplar ve iş parçacıkları arası posta kutusu.
add_library(gcs_core STATIC
  src/codec.cpp
  src/command_tracker.cpp
  src/geo.cpp
  src/mission.cpp
)
target_include_directories(gcs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gcs_core PRIVATE gcs_warnings)
set_target_properties(gcs_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
