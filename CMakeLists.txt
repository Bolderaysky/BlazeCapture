# --- --- --- --- --- --- --- --- SETUP PROJECT --- --- --- --- --- --- --- ---

cmake_minimum_required(VERSION 3.15)

project(BlazeCapture
        DESCRIPTION "Cross-platform application that allows you to do fast and efficient screen capturing with simple, intuitive and flexible interface"
        LANGUAGES CXX)

# --- --- --- --- --- --- --- --- INCLUDE CMAKE FEATURES --- --- --- --- --- --- --- ---

include(CheckIPOSupported)
include(cmake/AddGitSubmodule.cmake)

# --- --- --- --- --- --- --- --- SET VARIABLES AND OPTIONS --- --- --- --- --- --- --- ---

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCE_PATH "src" CACHE STRING "Source")
set(INCLUDE_PATH "include" CACHE STRING "Headers")
set(DEFAULT_BUILD_TYPE "Release" CACHE STRING "Define default build type")
set(IMGUI_PATH "dependencies/imgui" CACHE STRING "Path to Dear ImGui")
option(BUILD_TESTS "Boolean that specifies if it's needed to build tests or not" ON)

# --- --- --- --- --- --- --- --- PREVENT RUNNING CMAKE IN ROOT DIR --- --- --- --- --- --- --- ---

file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# --- --- --- --- --- --- --- --- IF BUILD TYPE IS NOT SET, SET TO RELEASE --- --- --- --- --- --- --- ---

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
        set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
        STRING "Choose the type of build." FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# --- --- --- --- --- --- --- --- SET COMPILER OPTIONS --- --- --- --- --- --- --- ---

if (UNIX)
        if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-Ofast -fno-exceptions -fno-rtti -fpermissive)
        endif()
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/Ot /Ox /Zc:auto /fp:fast /GT /GS- /Oi /GF /utf-8 /D_HAS_EXCEPTIONS=0 /MT /MTd)
        endif()
endif()

# --- --- --- --- --- --- --- --- INCLUDE, BUILD, LINK, ADD SUBDIRECTORIES --- --- --- --- --- --- --- ---

add_library(imgui STATIC
    $CACHE{IMGUI_PATH}/imgui.cpp
    $CACHE{IMGUI_PATH}/imgui_demo.cpp
    $CACHE{IMGUI_PATH}/imgui_draw.cpp
    $CACHE{IMGUI_PATH}/imgui_tables.cpp
    $CACHE{IMGUI_PATH}/imgui_widgets.cpp
    $CACHE{IMGUI_PATH}/backends/imgui_impl_glfw.cpp
    $CACHE{IMGUI_PATH}/backends/imgui_impl_vulkan.cpp
    $CACHE{IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
)

include_directories($CACHE{IMGUI_PATH})
include_directories($CACHE{IMGUI_PATH}/backends)

include_directories($CACHE{INCLUDE_PATH})

include_directories(${PROJECT_SOURCE_DIR}/dependencies/thread-pool)
include_directories(${PROJECT_SOURCE_DIR}/dependencies/hopscotch-map/include)

add_subdirectory($CACHE{SOURCE_PATH})

# if (BUILD_TESTS)
# add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
# endif()