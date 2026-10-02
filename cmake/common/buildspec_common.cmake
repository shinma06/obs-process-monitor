# Common build dependencies module
# Modified 2026-10-03: bind dependency reuse/search to verified local prefixes; verify archives,
# propagate the pinned toolset, and expose dependency build errors.

include_guard(GLOBAL)

# A previously downloaded archive must satisfy the same checksum as a fresh one.
function(_verify_dependency_archive archive expected_hash)
  file(SHA256 "${archive}" actual_hash)
  if(NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Dependency SHA256 mismatch: ${archive}. Remove this archive and retry; do not change the pinned hash.")
  endif()
endfunction()

# Only the managed extraction can satisfy a pinned dependency, never an external prefix.
function(_check_deps_version version prefix)
  set(found FALSE)
  if(EXISTS "${prefix}/share/obs-deps/VERSION")
    file(READ "${prefix}/share/obs-deps/VERSION" _check_version)
    string(STRIP "${_check_version}" _check_version)
    if(_check_version STREQUAL version)
      if(NOT dependency STREQUAL qt6 OR EXISTS "${prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        set(found TRUE)
      endif()
    endif()
  endif()
  return(PROPAGATE found)
endfunction()

# _setup_obs_studio: Create obs-studio build project, then build libobs and obs-frontend-api
function(_setup_obs_studio)
  if(OS_WINDOWS)
    set(_cmake_generator "${CMAKE_GENERATOR}")
    set(_cmake_arch "-A ${arch},version=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    set(_cmake_extra "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}" "-DENABLE_SCRIPTING=OFF" "-T${CMAKE_GENERATOR_TOOLSET}")
  elseif(OS_MACOS)
    set(_cmake_generator "Xcode")
    set(_cmake_arch "-DCMAKE_OSX_ARCHITECTURES:STRING='arm64;x86_64'")
    set(_cmake_extra "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()

  message(STATUS "Configure ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B
      "${dependencies_dir}/${_obs_destination}/build_${arch}" -G ${_cmake_generator} "${_cmake_arch}"
      -DOBS_CMAKE_VERSION:STRING=3.0.0 -DENABLE_PLUGINS:BOOL=OFF -DENABLE_FRONTEND:BOOL=OFF
      -DOBS_VERSION_OVERRIDE:STRING=${_obs_version} "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" --fresh
      ${_cmake_extra}
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
  )
  message(STATUS "Configure ${label} (${arch}) - done")

  message(STATUS "Build ${label} (Debug - ${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Debug --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
  )
  message(STATUS "Build ${label} (Debug - ${arch}) - done")

  message(STATUS "Build ${label} (Release - ${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Release --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
  )
  message(STATUS "Build ${label} (Reelase - ${arch}) - done")

  message(STATUS "Install ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Debug --prefix "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
  )
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Release --prefix "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
  )
  message(STATUS "Install ${label} (${arch}) - done")
endfunction()

# _check_dependencies: Fetch and extract pre-built OBS build dependencies
function(_check_dependencies)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

  string(JSON dependency_data GET ${buildspec} dependencies)
  set(pinned_prefixes)

  foreach(dependency IN LISTS dependencies_list)
    string(JSON data GET ${dependency_data} ${dependency})
    string(JSON version GET ${data} version)
    string(JSON hash GET ${data} hashes ${platform})
    string(JSON url GET ${data} baseUrl)
    string(JSON label GET ${data} label)
    string(JSON revision ERROR_VARIABLE error GET ${data} revision ${platform})

    message(STATUS "Setting up ${label} (${arch})")

    set(file "${${dependency}_filename}")
    set(destination "${${dependency}_destination}")
    string(REPLACE "VERSION" "${version}" file "${file}")
    string(REPLACE "VERSION" "${version}" destination "${destination}")
    string(REPLACE "ARCH" "${arch}" file "${file}")
    string(REPLACE "ARCH" "${arch}" destination "${destination}")
    if(revision)
      string(REPLACE "_REVISION" "_v${revision}" file "${file}")
      string(REPLACE "-REVISION" "-v${revision}" file "${file}")
    else()
      string(REPLACE "_REVISION" "" file "${file}")
      string(REPLACE "-REVISION" "" file "${file}")
    endif()

    set(prefix "${dependencies_dir}/${destination}")
    # Legacy checkout-wide markers do not establish which prefix was extracted.
    # Store the verified archive identity inside the managed extraction instead.
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      set(marker "${prefix}/.obs-plugin-dependency.sha256")
    else()
      set(marker "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256")
    endif()
    set(extracted_hash "")
    if(EXISTS "${marker}")
      file(READ "${marker}" extracted_hash)
    endif()

    if(dependency STREQUAL obs-studio)
      set(url ${url}/${file})
    else()
      set(url ${url}/${version}/${file})
    endif()

    if(NOT EXISTS "${dependencies_dir}/${file}")
      message(STATUS "Downloading ${url}")
      file(DOWNLOAD "${url}" "${dependencies_dir}/${file}" STATUS download_status EXPECTED_HASH SHA256=${hash})

      list(GET download_status 0 error_code)
      list(GET download_status 1 error_message)
      if(error_code GREATER 0)
        message(STATUS "Downloading ${url} - Failure")
        file(REMOVE "${dependencies_dir}/${file}")
        message(FATAL_ERROR "Unable to download ${url}, failed with error: ${error_message}")
      else()
        message(STATUS "Downloading ${url} - done")
      endif()
    endif()

    _verify_dependency_archive("${dependencies_dir}/${file}" "${hash}")

    set(reuse FALSE)
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      _check_deps_version("${version}" "${prefix}")
      if(extracted_hash STREQUAL hash AND found)
        set(reuse TRUE)
      endif()
    elseif(extracted_hash STREQUAL hash AND EXISTS "${prefix}")
      set(reuse TRUE)
    endif()

    if(NOT reuse)
      file(REMOVE_RECURSE "${prefix}")
      file(MAKE_DIRECTORY "${prefix}")
      if(dependency STREQUAL obs-studio)
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}")
      else()
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${prefix}")
        _check_deps_version("${version}" "${prefix}")
        if(NOT found)
          message(FATAL_ERROR "Pinned ${label} archive has an invalid VERSION or package layout: ${prefix}")
        endif()
      endif()
      file(WRITE "${marker}" "${hash}")
    endif()

    # Register the pinned prefix on every configure, including cache reuse.
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      list(APPEND pinned_prefixes "${prefix}")
      if(dependency STREQUAL qt6)
        set(qt_prefix "${prefix}")
      endif()
    elseif(dependency STREQUAL obs-studio)
      set(_obs_version ${version})
      set(_obs_destination "${destination}")
      list(APPEND pinned_prefixes "${dependencies_dir}")
    endif()

    message(STATUS "Setting up ${label} (${arch}) - done")
  endforeach()

  list(PREPEND CMAKE_PREFIX_PATH ${pinned_prefixes})
  list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "CMake prefix search path" FORCE)

  # find_package caches bypass prefix search. Discard stale Qt component locations;
  # anchor the top-level packages to the verified Qt and locally built OBS trees.
  get_cmake_property(cache_variables CACHE_VARIABLES)
  foreach(variable IN LISTS cache_variables)
    if(variable MATCHES "^Qt6.*_DIR$")
      unset(${variable} CACHE)
    endif()
  endforeach()
  set(Qt6_DIR "${qt_prefix}/lib/cmake/Qt6" CACHE PATH "Pinned Qt package" FORCE)
  set(libobs_DIR "${dependencies_dir}/cmake" CACHE PATH "Locally built libobs package" FORCE)
  set(obs-frontend-api_DIR "${dependencies_dir}/cmake" CACHE PATH "Locally built frontend package" FORCE)

  # OBS has its own find_package/library cache: configure it fresh on every run.
  _setup_obs_studio()
endfunction()
