# ---------------------------------------------------------------------------
# tracker_core: platform-independent logic. Builds everywhere and is what the
# unit tests link against.
# ---------------------------------------------------------------------------
configure_file(
    include/tracker/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/tracker/version.hpp
    @ONLY)

add_library(tracker_core STATIC
    src/cli.cpp)

target_include_directories(tracker_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

tracker_configure_target(tracker_core)

# ---------------------------------------------------------------------------
# tracker.exe: Windows-only GUI-subsystem executable (no console window).
# ---------------------------------------------------------------------------
if(WIN32)
    add_executable(tracker WIN32
        src/main_win.cpp)

    target_link_libraries(tracker PRIVATE tracker_core shell32 user32)
    target_compile_definitions(tracker PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    tracker_configure_target(tracker)
endif()
