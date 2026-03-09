cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(obs-process-monitor VERSION 1.0.0)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# OBS core
find_package(libobs REQUIRED)

# OBS Frontend API (required for dock creation)
find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

# Qt (required for QWidget/QDockWidget)
find_qt(COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

# Windows system libraries for process metrics
if(OS_WINDOWS)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE psapi)
endif()

# Source files (at repo root; use src/ if you move them under src/)
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    plugin-main.cpp
    ProcessMonitorWidget.cpp
    ProcessMonitorWidget.hpp)

# Link core OBS
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# Generate plugin macros header (output in build dir)
configure_file(plugin-macros.h.in ${CMAKE_BINARY_DIR}/plugin-macros.generated.h)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/plugin-macros.generated.h)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")

set_target_properties_obs(${CMAKE_PROJECT_NAME} PROPERTIES UI_AUTOMOC ON UI_AUTORCC ON UI_AUTOUIC ON)
