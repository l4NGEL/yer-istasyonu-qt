find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(gcs_core_tests
  core/test_codec.cpp
  core/test_golden.cpp
  core/test_mission.cpp
  core/test_command_tracker.cpp
  core/test_mailbox_geo.cpp
  core/test_sim_vehicle.cpp
)
target_include_directories(gcs_core_tests PRIVATE core)
target_link_libraries(gcs_core_tests PRIVATE gcs_core gcs_sitl_lib GTest::gtest_main)
find_package(Threads REQUIRED)
target_link_libraries(gcs_core_tests PRIVATE Threads::Threads)
gtest_discover_tests(gcs_core_tests DISCOVERY_TIMEOUT 30)

if(GCS_BUILD_APP)
  add_subdirectory(qt)
endif()
