# Firmware version injection
#
# Determines FIRMWARE_VERSION and adds it as a compile definition.
#   - If -DFIRMWARE_VERSION=<value> is passed on the cmake command line, that value is used.
#   - Otherwise, the short git hash is used (with "-dirty" appended if there are uncommitted changes).
#
# The C code can consume this via the FIRMWARE_VERSION compile definition or check the version.h header.

if(NOT DEFINED FIRMWARE_VERSION)
    execute_process(
        COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    execute_process(
        COMMAND git status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GIT_HASH STREQUAL "")
        set(FIRMWARE_VERSION "unknown")
    elseif(NOT GIT_STATUS STREQUAL "")
        set(FIRMWARE_VERSION "${GIT_HASH}-dirty")
    else()
        set(FIRMWARE_VERSION "${GIT_HASH}")
    endif()
endif()

message(STATUS "Firmware version: ${FIRMWARE_VERSION}")
target_compile_definitions(${BUILD_UNIT_0_NAME} PRIVATE FIRMWARE_VERSION="${FIRMWARE_VERSION}")
