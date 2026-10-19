cmake_minimum_required(VERSION 3.22)
project(yer_istasyonu_qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GCS_BUILD_APP "Qt 6 / QML yer istasyonu uygulamasını derle" ON)
option(GCS_BUILD_TESTS "Birim ve entegrasyon testlerini derle" ON)
option(GCS_WITH_GSTREAMER "Video alımı için GStreamer kullan" ON)
option(GCS_WERROR "Uyarıları hata say" OFF)
set(GCS_SANITIZER "" CACHE STRING "Boş, 'thread' ya da 'address'")

add_library(gcs_warnings INTERFACE)
target_compile_options(gcs_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                              -Wno-sign-conversion)
if(GCS_WERROR)
  target_compile_options(gcs_warnings INTERFACE -Werror)
endif()

if(GCS_SANITIZER STREQUAL "thread")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=thread)
elseif(GCS_SANITIZER STREQUAL "address")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(core)
add_subdirectory(sitl)

if(GCS_BUILD_APP)
  add_subdirectory(app)
endif()

if(GCS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
