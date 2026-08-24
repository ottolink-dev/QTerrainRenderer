cmake_minimum_required(VERSION 3.16)
project(
  qterrain_renderer-root
  VERSION 0.0.0
  LANGUAGES CXX)

# --- Global output paths

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Default build type

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting default build type to Release")
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
endif()

message(STATUS "Release type: ${CMAKE_BUILD_TYPE}")

# --- C++ standard

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Interface target for compiler options & definitions

add_library(qtr_compiler_flags INTERFACE)

target_compile_definitions(
  qtr_compiler_flags
  INTERFACE $<$<CONFIG:Debug>:DEBUG_BUILD> $<$<CONFIG:Release>:RELEASE_BUILD>
            $<$<BOOL:${UNIX}>:QTR_OS_LINUX> $<$<CONFIG:Debug>:LOG_LEVEL=3>
            $<$<CONFIG:Release>:LOG_LEVEL=3>)

if(MSVC)
  target_compile_options(
    qtr_compiler_flags INTERFACE /W4 $<$<CONFIG:Debug>:/Od> /D_USE_MATH_DEFINES)
else()
  target_compile_options(
    qtr_compiler_flags
    INTERFACE -Wall
              -Wextra
              -Wno-dangling-reference
              -pthread
              -Wno-deprecated-declarations
              $<$<CONFIG:Release>:-O3>
              $<$<CONFIG:Debug>:-g>
              $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
              $<$<CONFIG:Debug>:-Wpedantic>
              $<$<CONFIG:Debug>:-Wno-psabi>
              $<$<CONFIG:Debug>:-Wno-unused-parameter>
              $<$<CONFIG:Debug>:-Wcast-align>
              $<$<CONFIG:Debug>:-Wcast-qual>
              $<$<CONFIG:Debug>:-Wnull-dereference>
              $<$<CONFIG:Debug>:-Wnon-virtual-dtor>
              $<$<CONFIG:Debug>:-Woverloaded-virtual>
              $<$<CONFIG:Debug>:-Wzero-as-null-pointer-constant>
              $<$<CONFIG:Debug>:-Wduplicated-cond>
              $<$<CONFIG:Debug>:-Wduplicated-branches>
              $<$<CONFIG:Debug>:-Wlogical-op>
              $<$<CONFIG:Debug>:-Wuseless-cast>)
endif()

# --- Dependencies

find_package(nlohmann_json 3.11 REQUIRED)
find_package(spdlog REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
find_package(glfw3 CONFIG REQUIRED)

add_subdirectory(external)
add_subdirectory(QTerrainRenderer)
add_subdirectory(tests)
