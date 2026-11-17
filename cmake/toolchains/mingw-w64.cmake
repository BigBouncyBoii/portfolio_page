# Cross-compile tracker.exe for Windows from Linux / WSL.
#   sudo apt install mingw-w64
#   cmake --preset mingw && cmake --build --preset mingw

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Prefer the POSIX thread model variant: it supports std::thread / std::mutex,
# which later phases rely on.
find_program(CMAKE_CXX_COMPILER NAMES ${TOOLCHAIN_PREFIX}-g++-posix ${TOOLCHAIN_PREFIX}-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
