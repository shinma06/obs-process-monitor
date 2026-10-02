cmake_minimum_required(VERSION 4.4.3)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The OBS plugin supports Windows x64 only.")
endif()

if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "19.51.36260.0")
  message(FATAL_ERROR "Use the pinned MSVC 19.51.36260.0 toolchain; record and review toolchain upgrades.")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)
find_package(libobs 32.2.2 REQUIRED)
find_package(obs-frontend-api 32.2.2 REQUIRED)
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Widgets)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets psapi)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE NOMINMAX)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE plugin-main.cpp ProcessMonitorWidget.cpp ProcessMonitorWidget.hpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE PLUGIN_SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
configure_file(plugin-macros.h.in plugin-macros.generated.h @ONLY)
configure_file(cmake/build-info.json.in build-info.json @ONLY)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
