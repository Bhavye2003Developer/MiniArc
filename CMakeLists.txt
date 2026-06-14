cmake_minimum_required(VERSION 3.21)
project(miniARC VERSION 0.1.0 LANGUAGES CXX)

# Enable Objective-C++ on macOS for thermal_macos.mm
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Fetch llama.cpp ──────────────────────────────────────────────────────────
# Check https://github.com/ggerganov/llama.cpp/releases for the latest tag
# and update GIT_TAG if b5891 is no longer the latest stable.
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
    set_source_files_properties(src/thermal/thermal_macos.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(miniARC_core PUBLIC "-framework Foundation")
elseif(WIN32)
    target_sources(miniARC_core PRIVATE src/thermal/thermal_windows.cpp)
    target_link_libraries(miniARC_core PUBLIC wbemuuid ole32 oleaut32 PowrProf)
endif()

target_include_directories(miniARC_core PUBLIC src)
target_link_libraries(miniARC_core PUBLIC llama)

# Disable llama.cpp log spam by default
target_compile_definitions(miniARC_core PUBLIC GGML_USE_LLAMAFILE=0)

# ── CLI executable ────────────────────────────────────────────────────────────
add_executable(miniARC src/main.cpp)
target_link_libraries(miniARC PRIVATE miniARC_core)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
