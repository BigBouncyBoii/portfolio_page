# ---------------------------------------------------------------------------
# tracker_core: platform-independent logic. Builds everywhere and is what the
# unit tests link against.
# ---------------------------------------------------------------------------
configure_file(
    include/tracker/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/tracker/version.hpp
    @ONLY)

# SQLite: single-file amalgamation, compiled as a static C library.
include(FetchContent)
FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0)

add_library(tracker_core STATIC
    src/cli.cpp
    src/key_counter.cpp
    src/storage.cpp)

target_include_directories(tracker_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(tracker_core PRIVATE sqlite3)

tracker_configure_target(tracker_core)

# ---------------------------------------------------------------------------
# tracker.exe: Windows-only GUI-subsystem executable (no console window).
# ---------------------------------------------------------------------------
if(WIN32)
    # tracker_win: Win32 collectors (keyboard hook; later the focus sampler).
    add_library(tracker_win STATIC
        src/keyboard_hook_win.cpp
        src/focus_sampler_win.cpp)
    target_include_directories(tracker_win PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tracker_win PUBLIC tracker_core user32)
    target_compile_definitions(tracker_win PUBLIC
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    tracker_configure_target(tracker_win)

    add_executable(tracker WIN32
        src/main_win.cpp)

    target_link_libraries(tracker PRIVATE tracker_win shell32)

    tracker_configure_target(tracker)
endif()
