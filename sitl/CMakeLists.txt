add_library(gcs_sitl_lib STATIC sim_vehicle.cpp)
target_include_directories(gcs_sitl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gcs_sitl_lib PUBLIC gcs_core PRIVATE gcs_warnings)

add_executable(gcs-sitl sitl_main.cpp)
target_link_libraries(gcs-sitl PRIVATE gcs_sitl_lib gcs_warnings)
