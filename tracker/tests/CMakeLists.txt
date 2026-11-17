include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(tracker_tests
    cli_test.cpp)

target_link_libraries(tracker_tests PRIVATE tracker_core GTest::gtest_main)
tracker_configure_target(tracker_tests)

include(GoogleTest)
gtest_discover_tests(tracker_tests)
