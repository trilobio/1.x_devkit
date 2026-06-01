# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Project definition
project(TriloOpenBootloader)

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(BUILD_UNIT_0_NAME TriloOpenBootloader)

# Create an executable object type
add_executable(${BUILD_UNIT_0_NAME})

# STM32CubeIDE for Visual Studio Code managed CMake file
include(cmake/TriloOpenBootloader.cmake)

# Version handling
include(cmake/version.cmake)

# Compiler options
target_compile_options(${BUILD_UNIT_0_NAME} PRIVATE
  # User defined compiler options
)

# Linker options
target_link_options(${BUILD_UNIT_0_NAME} PRIVATE 
  # User defined linker options
)

# Add sources to executable/library
target_sources(${BUILD_UNIT_0_NAME} PRIVATE
  # User defined sources
)

# Add include directories
target_include_directories(${BUILD_UNIT_0_NAME} PRIVATE
  # User defined include directories
)

# Generate hex and bin files with TriloOpenBootloader-<VARIANT>-<VERSION> naming pattern
add_custom_command(TARGET ${BUILD_UNIT_0_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${BUILD_UNIT_0_NAME}> ${CMAKE_BINARY_DIR}/${BUILD_UNIT_0_NAME}-${BOARD_VARIANT}-${FIRMWARE_VERSION}.elf
  COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${BUILD_UNIT_0_NAME}> ${CMAKE_BINARY_DIR}/${BUILD_UNIT_0_NAME}-${BOARD_VARIANT}-${FIRMWARE_VERSION}.hex
  COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${BUILD_UNIT_0_NAME}> ${CMAKE_BINARY_DIR}/${BUILD_UNIT_0_NAME}-${BOARD_VARIANT}-${FIRMWARE_VERSION}.bin
  COMMAND ${CMAKE_COMMAND} -E echo "Generated versioned firmware: ${BUILD_UNIT_0_NAME}-${BOARD_VARIANT}-${FIRMWARE_VERSION}.{elf,hex,bin}"
  COMMENT "Generate versioned firmware files"
  VERBATIM
)
