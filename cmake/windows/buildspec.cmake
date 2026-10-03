# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

option(OBS_PLUGIN_PACKAGE_BUILD "Use private dependency extraction/build/install for an identified package" OFF)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  set(arch ${CMAKE_VS_PLATFORM_NAME})
  set(platform windows-${arch})

  # Archive bytes may be shared, but identified packages must not trust a
  # developer's extracted sources, prebuilt files, objects or installed libraries.
  set(dependency_archives_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  if(OBS_PLUGIN_PACKAGE_BUILD)
    set(dependencies_dir "${CMAKE_CURRENT_BINARY_DIR}/.deps")
  else()
    set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  endif()
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()
endfunction()

_check_dependencies_windows()
