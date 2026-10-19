find_package(Qt6 REQUIRED COMPONENTS Test)

function(gcs_qt_test name)
  add_executable(${name} ${name}.cpp FakeLink.h)
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE gcs_qt Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
  # Testler pencere açmaz; GUI eklentisi gerektiren yerlerde ekransız çalışsın
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
endfunction()

gcs_qt_test(tst_udplink)
gcs_qt_test(tst_vehicle_payload)
gcs_qt_test(tst_missionmodel)
gcs_qt_test(tst_video)
gcs_qt_test(tst_sitl_e2e)
target_compile_definitions(tst_sitl_e2e PRIVATE GCS_SITL_PATH="$<TARGET_FILE:gcs-sitl>")
add_dependencies(tst_sitl_e2e gcs-sitl)
# Sabit UDP portları kullanır; paralel ctest'te çakışmasın
set_tests_properties(tst_sitl_e2e tst_video PROPERTIES RUN_SERIAL TRUE)
