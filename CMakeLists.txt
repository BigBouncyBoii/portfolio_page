cmake_minimum_required(VERSION 3.21)

project(PortfolioTracker
    VERSION 0.1.0
    DESCRIPTION "Lightweight PC activity tracker that publishes stats to a portfolio page"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime so tracker.exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Put all executables in one predictable place: build/<preset>/bin[/<Config>]
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(TRACKER_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(TRACKER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

include(cmake/CompilerOptions.cmake)

add_subdirectory(tracker)

if(TRACKER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tracker/tests)
endif()
