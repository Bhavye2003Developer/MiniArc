FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_miniARC
    test_scheduler.cpp
    test_thermal_logic.cpp
)
target_link_libraries(test_miniARC PRIVATE miniARC_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(test_miniARC)
