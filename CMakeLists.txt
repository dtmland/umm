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

add_library(umm-cli-core STATIC
  src/args.cpp
  src/batch.cpp
  src/cli.cpp
  src/command_table.cpp
  src/commands.cpp
  src/config.cpp
  src/errors.cpp
  src/output.cpp
  src/property.cpp
  src/value_format.cpp)
target_include_directories(umm-cli-core PUBLIC src)
target_link_libraries(umm-cli-core PUBLIC umm::umm)
target_compile_definitions(umm-cli-core PUBLIC
  "UMM_CLI_VERSION=\"${PROJECT_VERSION}\""
  "UMM_CLI_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
if(WIN32)
  target_compile_definitions(umm-cli-core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/exiftool.env")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/exiftool.env" _et_pin_lines
       REGEX "^UMM_EXIFTOOL_VERSION=")
  foreach(_line IN LISTS _et_pin_lines)
    string(REGEX MATCH "^([A-Z0-9_]+)=(.*)$" _m "${_line}")
    if(CMAKE_MATCH_1 STREQUAL "UMM_EXIFTOOL_VERSION" AND CMAKE_MATCH_2)
      target_compile_definitions(umm-cli-core PUBLIC
        "UMM_EXIFTOOL_TESTED_VERSION=\"${CMAKE_MATCH_2}\"")
    endif()
  endforeach()
endif()

add_executable(umm-cli src/main.cpp)
set_target_properties(umm-cli PROPERTIES OUTPUT_NAME umm)
target_link_libraries(umm-cli PRIVATE umm-cli-core)

enable_testing()
add_executable(umm-cli-tests tests/cli/test_cli.cpp)
target_link_libraries(umm-cli-tests PRIVATE umm-cli-core)
if(NOT UMM_CLI_USE_SYSTEM_LIBUMM AND DEFINED libumm_SOURCE_DIR)
  target_compile_definitions(umm-cli-tests PRIVATE
    "UMM_LIBUMM_METADATA_HPP=\"${libumm_SOURCE_DIR}/include/umm/metadata.hpp\""
    "UMM_LIBUMM_FIXTURES=\"${libumm_SOURCE_DIR}/tests/fixtures\"")
endif()
add_test(NAME umm_cli_unit COMMAND umm-cli-tests)

add_test(NAME umm_version COMMAND umm-cli version)
add_test(NAME umm_help COMMAND umm-cli --help)
add_test(NAME umm_no_command COMMAND umm-cli)
set_tests_properties(umm_no_command PROPERTIES WILL_FAIL TRUE)
add_test(NAME umm_unknown_command COMMAND umm-cli not-a-command)
set_tests_properties(umm_unknown_command PROPERTIES WILL_FAIL TRUE)
add_test(NAME umm_batch_missing_files
         COMMAND umm-cli read --json --recursive no-such-1.jpg no-such-2.jpg)
set_tests_properties(umm_batch_missing_files PROPERTIES WILL_FAIL TRUE)
