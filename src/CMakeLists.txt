cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE _SOURCES LIST_DIRECTORIES false *.cpp)
set(SOURCES ${_SOURCES})
add_library(BlazeCapture ${_SOURCES})
target_link_libraries(BlazeCapture PUBLIC imgui vulkan glfw)

add_executable(BlazeCaptureApp "app/main.cpp")
target_link_libraries(BlazeCaptureApp PRIVATE BlazeCapture obs)

set_property(TARGET BlazeCaptureApp PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})