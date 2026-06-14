cmake_minimum_required(VERSION 3.21)
project(miniARC VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Enable Objective-C++ on macOS for thermal_macos.mm
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Fetch llama.cpp ──────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    llama
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG        b5891
    GIT_SHALLOW    TRUE
)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(llama)

# ── Fetch cpp-httplib (header-only HTTP server) ───────────────────────────────
# Fetch httplib header-only — bypass its CMakeLists to avoid OpenSSL auto-detection.
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.7
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(httplib)
if(NOT httplib_POPULATED)
    FetchContent_Populate(httplib)
endif()

# ── Core library ─────────────────────────────────────────────────────────────
add_library(miniARC_core STATIC
    src/thermal/thermal_monitor.cpp
    src/scheduler/scheduler.cpp
    src/engine/engine.cpp
)

# Platform-specific thermal source
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(miniARC_core PRIVATE src/thermal/thermal_linux.cpp)
elseif(APPLE)
    target_sources(miniARC_core PRIVATE src/thermal/thermal_macos.mm)
    target_link_libraries(miniARC_core PRIVATE "-framework Foundation")
elseif(WIN32)
    target_sources(miniARC_core PRIVATE src/thermal/thermal_windows.cpp)
    target_link_libraries(miniARC_core PRIVATE wbemuuid ole32 oleaut32 PowrProf)
endif()

target_include_directories(miniARC_core PUBLIC src)
target_link_libraries(miniARC_core PUBLIC llama)

# ── CLI + web server executable ──────────────────────────────────────────────
add_executable(miniARC src/main.cpp src/server/server.cpp)
target_include_directories(miniARC PRIVATE ${httplib_SOURCE_DIR})
# httplib enables optional backends only when their macros are explicitly defined.
# Defining nothing here means plain HTTP only — no OpenSSL/zlib needed.
target_link_libraries(miniARC PRIVATE miniARC_core)
if(WIN32)
    target_link_libraries(miniARC PRIVATE ws2_32)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
