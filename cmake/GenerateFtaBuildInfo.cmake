if ( NOT DEFINED FTA_SOURCE_DIR OR "${FTA_SOURCE_DIR}" STREQUAL "" )
  message ( FATAL_ERROR "FTA_SOURCE_DIR is required." )
endif ()

if ( NOT DEFINED FTA_OUTPUT_FILE OR "${FTA_OUTPUT_FILE}" STREQUAL "" )
  message ( FATAL_ERROR "FTA_OUTPUT_FILE is required." )
endif ()

set ( _fta_build_date "$ENV{FTA_BUILD_DATE}" )
if ( "${_fta_build_date}" STREQUAL "" )
  string ( TIMESTAMP _fta_build_date "%Y%m%d" UTC )
elseif ( NOT "${_fta_build_date}" MATCHES "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]$" )
  message ( FATAL_ERROR "FTA_BUILD_DATE must use UTC YYYYMMDD format, got: ${_fta_build_date}" )
endif ()

set ( _fta_commit "unknown" )
set ( _fta_dirty "" )
find_program ( GIT_EXECUTABLE git )
if ( GIT_EXECUTABLE )
  execute_process (
    COMMAND "${GIT_EXECUTABLE}" -C "${FTA_SOURCE_DIR}" rev-parse --short=8 HEAD
    OUTPUT_VARIABLE _fta_commit_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _fta_commit_result
  )
  if ( _fta_commit_result EQUAL 0 AND "${_fta_commit_output}" MATCHES "^[0-9a-fA-F]+$" )
    string ( TOLOWER "${_fta_commit_output}" _fta_commit )
  endif ()

  execute_process (
    COMMAND "${GIT_EXECUTABLE}" -C "${FTA_SOURCE_DIR}" status --porcelain --untracked-files=no
    OUTPUT_VARIABLE _fta_status_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _fta_status_result
  )
  if ( _fta_status_result EQUAL 0 AND NOT "${_fta_status_output}" STREQUAL "" )
    set ( _fta_dirty "-dirty" )
  endif ()
endif ()

set ( _fta_build_id "${_fta_build_date}-${_fta_commit}${_fta_dirty}" )
set ( _fta_content "#pragma once\n\n#define FTA_BUILD_ID \"${_fta_build_id}\"\n#define FTA_BUILD_DATE \"${_fta_build_date}\"\n" )

get_filename_component ( _fta_output_dir "${FTA_OUTPUT_FILE}" DIRECTORY )
file ( MAKE_DIRECTORY "${_fta_output_dir}" )

set ( _fta_existing "" )
if ( EXISTS "${FTA_OUTPUT_FILE}" )
  file ( READ "${FTA_OUTPUT_FILE}" _fta_existing )
endif ()

if ( NOT "${_fta_existing}" STREQUAL "${_fta_content}" )
  file ( WRITE "${FTA_OUTPUT_FILE}" "${_fta_content}" )
endif ()

message ( STATUS "FTA build ID: ${_fta_build_id}" )
