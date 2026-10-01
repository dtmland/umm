cmake_minimum_required(VERSION 3.21)
project(umm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UMM_CLI_USE_SYSTEM_LIBUMM
  "Use an installed libumm via find_package(umm CONFIG) instead of the pinned source archive" OFF)

if(UMM_CLI_USE_SYSTEM_LIBUMM)
  find_package(umm CONFIG REQUIRED)
else()
  include(FetchContent)
  # Pins live in tools/build/libumm.env (source of truth).
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/libumm.env" _umm_pin_lines
       REGEX "^UMM_LIBUMM_[A-Z0-9_]+=")
  foreach(_line IN LISTS _umm_pin_lines)
    string(REGEX MATCH "^([A-Z0-9_]+)=(.*)$" _m "${_line}")
    set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endforeach()
  foreach(_v UMM_LIBUMM_VERSION UMM_LIBUMM_URL UMM_LIBUMM_SHA256)
    if(NOT ${_v})
      message(FATAL_ERROR "tools/build/libumm.env does not define ${_v}")
    endif()
  endforeach()

  # Static libumm; its own build acquires its backends.
  set(BUILD_SHARED_LIBS OFF)
  set(UMM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(libumm
    URL "${UMM_LIBUMM_URL}"
    URL_HASH SHA256=${UMM_LIBUMM_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(libumm)
endif()

add_executable(umm-cli src/main.cpp)
set_target_properties(umm-cli PROPERTIES OUTPUT_NAME umm)
target_link_libraries(umm-cli PRIVATE umm::umm)
