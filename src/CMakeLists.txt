cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

include_directories(${PIPEWIRE_INCLUDE_DIRS})

file(GLOB_RECURSE _SOURCES LIST_DIRECTORIES false *.cpp)
set(SOURCES ${_SOURCES})
add_library(BlazeCapture ${_SOURCES})
target_link_libraries(BlazeCapture PUBLIC imgui vulkan glfw)

add_executable(BlazeCaptureApp "app/main.cpp" ${CMAKE_BINARY_DIR}/NvFBCUtils.o)
target_link_libraries(BlazeCaptureApp PRIVATE BlazeCapture ${PKG_PipeWire_LIBRARY_DIRS} X11 GLU GL pipewire-0.3 xcb xcb-image Xext xcb-shm yuv xcb-randr SQLiteCpp sqlite3)

set_property(TARGET BlazeCaptureApp PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})