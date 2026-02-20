# ==============================================================================
# Nand2Tetris Educational Suite - CMake Build Configuration
# ==============================================================================
# This file defines how to build the entire project:
# - C++ core simulation engines
# - Command-line tools
# - Test suite
# - WebAssembly bindings (optional)
# ==============================================================================

# Minimum CMake version required
# 3.15 gives us better support for modern C++ and cleaner syntax
cmake_minimum_required(VERSION 3.15)

# Project definition
# This sets up the project name and enables C++ language support
project(Nand2TetrisSuite
    VERSION 1.0.0
    DESCRIPTION "Educational simulation and debugging suite for Nand2Tetris"
    LANGUAGES CXX
)

# ==============================================================================
# C++ Standard Settings
# ==============================================================================
# We use C++17 for modern features while maintaining broad compatibility:
# - std::optional for optional values
# - std::string_view for efficient string handling
# - Structured bindings
# - if constexpr for compile-time conditionals

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Use standard C++, not compiler extensions

# ==============================================================================
# Build Type Configuration
# ==============================================================================
# Default to Release build if not specified
# Debug: Full debugging symbols, no optimization, assertions enabled
# Release: Optimized, no debug symbols, assertions disabled (fast!)
# RelWithDebInfo: Optimized but with debug symbols (for profiling)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

# ==============================================================================
# Output Directories
# ==============================================================================
# Put all compiled binaries and libraries in convenient locations
# This keeps the build directory clean and organized

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # Executables go here
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # Shared libraries (.so, .dll)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # Static libraries (.a, .lib)

# ==============================================================================
# Compiler Flags
# ==============================================================================
# These flags enable important warnings and optimizations

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC and Clang flags
    add_compile_options(
        -Wall              # Enable most warnings
        -Wextra            # Enable extra warnings
        -Wpedantic         # Warn about non-standard C++
        -Wconversion       # Warn about implicit type conversions
        -Wshadow           # Warn about variable shadowing
        -Wunused           # Warn about unused variables/functions
        -Wno-unknown-pragmas  # Don't warn about pragma directives
    )

    # Additional flags for Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
            -O3            # Maximum optimization
            -DNDEBUG       # Disable assertions
            -march=native  # Optimize for this CPU architecture
        )
    endif()

    # Additional flags for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(
            -g             # Full debug symbols
            -O0            # No optimization (easier debugging)
        )
    endif()

elseif(MSVC)
    # Microsoft Visual C++ flags
    add_compile_options(
        /W4                # Warning level 4 (high)
        /permissive-       # Strict standards conformance
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
            /O2            # Maximize speed
            /DNDEBUG       # Disable assertions
        )
    endif()
endif()

# ==============================================================================
# Project Options
# ==============================================================================
# These options allow users to customize the build

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_WEB "Build WebAssembly bindings" OFF)
option(BUILD_EXAMPLES "Build example programs" ON)

# ==============================================================================
# Include Directories
# ==============================================================================
# Make core headers available to all targets

include_directories(
    ${CMAKE_SOURCE_DIR}/core/common
    ${CMAKE_SOURCE_DIR}/core/cpu
    ${CMAKE_SOURCE_DIR}/core/vm
    ${CMAKE_SOURCE_DIR}/core/jack
    ${CMAKE_SOURCE_DIR}/core/hdl
    ${CMAKE_SOURCE_DIR}/core/api
)

# ==============================================================================
# Core Libraries
# ==============================================================================
# These are the simulation engines that do the actual work

# Common utilities library (shared by all components)
add_subdirectory(core/common)

# CPU simulator library
add_subdirectory(core/cpu)

# VM emulator library
add_subdirectory(core/vm)

# Jack debugger library
add_subdirectory(core/jack)

# HDL simulator library
add_subdirectory(core/hdl)

# C API for language bindings
add_subdirectory(core/api)

# ==============================================================================
# Command-Line Tools
# ==============================================================================
# User-facing executables for each simulator

add_subdirectory(cli)

# ==============================================================================
# Tests
# ==============================================================================
# Unit and integration tests

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==============================================================================
# WebAssembly
# ==============================================================================
# Optional WebAssembly build for browser

if(BUILD_WEB)
    # This requires Emscripten toolchain
    # Configure with: emcmake cmake ..
    add_subdirectory(web/wasm)
endif()

# ==============================================================================
# Examples
# ==============================================================================
# Example programs for testing and demonstration

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ==============================================================================
# Installation
# ==============================================================================
# Define what gets installed and where

install(
    TARGETS cpu_sim vm_emu jack_debug hdl_sim
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install header files for library users
install(
    DIRECTORY core/
    DESTINATION include/nand2tetris
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ==============================================================================
# Print Configuration Summary
# ==============================================================================

message(STATUS "")
message(STATUS "==================================================")
message(STATUS "Nand2Tetris Suite Build Configuration")
message(STATUS "==================================================")
message(STATUS "Build type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:       C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "  Build tests:      ${BUILD_TESTS}")
message(STATUS "  Build web:        ${BUILD_WEB}")
message(STATUS "  Build examples:   ${BUILD_EXAMPLES}")
message(STATUS "")
message(STATUS "Output directories:")
message(STATUS "  Executables:      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Libraries:        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "==================================================")
message(STATUS "")
