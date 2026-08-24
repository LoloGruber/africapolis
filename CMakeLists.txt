cmake_minimum_required(VERSION 3.16)
project(africapolis_workflow VERSION 1.5.0 LANGUAGES CXX)

# Variables
set(CMAKE_CXX_STANDARD 23)

# Compiler flags and warnings
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "Setting compiler flags and warnings")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter -Wno-unused-function)
endif()

# Include Dependencies
include(lib/fishnet.cmake)
include(lib/cli11.cmake)
include(lib/magic_enum.cmake)
include(lib/spdlog.cmake)
# Build sources
add_subdirectory(src)
