project(qterrain-renderer)

# Qt6
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_VERBOSE ON)

file(GLOB_RECURSE QTR_HEADERS CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

file(GLOB_RECURSE QTR_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# --- Embedded shaders generation

file(GLOB_RECURSE QTR_SHADER_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.*)

set(EMBEDDED_SHADERS_CPP ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.cpp)

add_custom_command(
  OUTPUT ${EMBEDDED_SHADERS_CPP}
  COMMAND
    ${CMAKE_COMMAND} -DSHADERS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/shaders
    -DOUTPUT_FILE=${EMBEDDED_SHADERS_CPP} -P
    ${CMAKE_SOURCE_DIR}/cmake/embed_shaders.cmake
  DEPENDS ${QTR_SHADER_FILES} ${CMAKE_SOURCE_DIR}/cmake/embed_shaders.cmake
  COMMENT "Generating embedded shaders source"
  VERBATIM)

add_library(${PROJECT_NAME} STATIC ${QTR_SOURCES} ${QTR_HEADERS}
                                   ${EMBEDDED_SHADERS_CPP})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include>)

# Copy asset data to binary output directories
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_BINARY_DIR}/.)

# Link libraries
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC qtr_compiler_flags
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Qt6::Core
         Qt6::Widgets
         Qt6::OpenGL
         Qt6::OpenGLWidgets
         imgui::imgui
         glfw)
