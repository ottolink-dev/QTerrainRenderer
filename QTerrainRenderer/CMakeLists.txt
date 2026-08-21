project(qterrain-renderer)

# Qt6
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_VERBOSE ON)

file(GLOB_RECURSE QTR_HEADERS CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

file(GLOB_RECURSE QTR_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${QTR_SOURCES} ${QTR_HEADERS})
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
