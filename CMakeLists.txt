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
find_package(Threads REQUIRED)

add_library(resource-metrics STATIC src/metrics/MetricMath.cpp src/metrics/ResourceSampler.cpp)
target_include_directories(resource-metrics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(resource-metrics PRIVATE psapi pdh dxgi)

add_library(${CMAKE_PROJECT_NAME} MODULE)
find_package(libobs 32.2.2 REQUIRED)
find_package(obs-frontend-api 32.2.2 REQUIRED)
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Widgets)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets psapi resource-metrics Threads::Threads)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE NOMINMAX)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  plugin-main.cpp
  ProcessMonitorWidget.cpp
  ProcessMonitorWidget.hpp
  src/ui/MetricPresentation.cpp
  src/ui/MetricPresentation.hpp
  src/ui/SnapshotWorker.cpp
  src/ui/SnapshotWorker.hpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE PLUGIN_SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
configure_file(plugin-macros.h.in plugin-macros.generated.h @ONLY)
configure_file(cmake/build-info.json.in build-info.json @ONLY)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

include(CTest)
if(BUILD_TESTING)
  add_executable(metric-math-tests tests/metrics/MetricMathTests.cpp src/metrics/MetricMath.cpp)
  target_include_directories(metric-math-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME metric-math COMMAND metric-math-tests)

  add_executable(sampler-smoke tests/metrics/SamplerSmoke.cpp)
  target_link_libraries(sampler-smoke PRIVATE resource-metrics)
  add_test(NAME sampler-smoke COMMAND sampler-smoke)
  set_tests_properties(sampler-smoke PROPERTIES TIMEOUT 60)

  add_executable(ui-presentation-tests tests/ui/test_metric_presentation.cpp src/ui/MetricPresentation.cpp)
  target_include_directories(ui-presentation-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(ui-presentation-tests PRIVATE NOMINMAX)
  target_link_libraries(ui-presentation-tests PRIVATE Qt6::Core)
  add_test(NAME ui-presentation COMMAND ui-presentation-tests)
  set_tests_properties(ui-presentation PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 15)

  add_executable(ui-worker-tests tests/ui/test_snapshot_worker.cpp src/ui/SnapshotWorker.cpp)
  target_include_directories(ui-worker-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(ui-worker-tests PRIVATE Threads::Threads)
  add_test(NAME ui-worker COMMAND ui-worker-tests)
  set_tests_properties(ui-worker PROPERTIES TIMEOUT 15)
endif()
