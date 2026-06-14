#include <catch2/catch_test_macros.hpp>
#include "thermal/thermal.h"

TEST_CASE("temp_to_state: basic thresholds from COOL") {
    REQUIRE(temp_to_state(50.0f, ThermalState::COOL)  == ThermalState::COOL);
    REQUIRE(temp_to_state(69.9f, ThermalState::COOL)  == ThermalState::COOL);
    REQUIRE(temp_to_state(70.0f, ThermalState::COOL)  == ThermalState::WARM);
    REQUIRE(temp_to_state(79.9f, ThermalState::COOL)  == ThermalState::WARM);
    REQUIRE(temp_to_state(80.0f, ThermalState::COOL)  == ThermalState::HOT);
    REQUIRE(temp_to_state(84.9f, ThermalState::COOL)  == ThermalState::HOT);
    REQUIRE(temp_to_state(85.0f, ThermalState::COOL)  == ThermalState::CRITICAL);
    REQUIRE(temp_to_state(100.0f, ThermalState::COOL) == ThermalState::CRITICAL);
}

TEST_CASE("temp_to_state: HOT->WARM hysteresis -- stays HOT between 78-80 degrees C") {
    REQUIRE(temp_to_state(79.0f, ThermalState::HOT) == ThermalState::HOT);
    REQUIRE(temp_to_state(78.0f, ThermalState::HOT) == ThermalState::HOT);
}

TEST_CASE("temp_to_state: HOT->WARM hysteresis -- exits HOT below 78 degrees C") {
    REQUIRE(temp_to_state(77.9f, ThermalState::HOT) == ThermalState::WARM);
    REQUIRE(temp_to_state(50.0f, ThermalState::HOT) == ThermalState::COOL);
}

TEST_CASE("temp_to_state: WARM->COOL hysteresis -- stays WARM between 68-70 degrees C") {
    REQUIRE(temp_to_state(69.0f, ThermalState::WARM) == ThermalState::WARM);
    REQUIRE(temp_to_state(68.0f, ThermalState::WARM) == ThermalState::WARM);
}

TEST_CASE("temp_to_state: WARM->COOL hysteresis -- exits WARM below 68 degrees C") {
    REQUIRE(temp_to_state(67.9f, ThermalState::WARM) == ThermalState::COOL);
}

TEST_CASE("temp_to_state: CRITICAL->HOT hysteresis -- stays CRITICAL between 83-85 degrees C") {
    REQUIRE(temp_to_state(84.0f, ThermalState::CRITICAL) == ThermalState::CRITICAL);
    REQUIRE(temp_to_state(83.0f, ThermalState::CRITICAL) == ThermalState::CRITICAL);
}

TEST_CASE("temp_to_state: CRITICAL->HOT hysteresis -- exits CRITICAL below 83 degrees C") {
    REQUIRE(temp_to_state(82.9f, ThermalState::CRITICAL) == ThermalState::HOT);
}

TEST_CASE("temp_to_state: no hysteresis from COOL state") {
    // From COOL, transitions are immediate at thresholds
    REQUIRE(temp_to_state(70.0f, ThermalState::COOL) == ThermalState::WARM);
    REQUIRE(temp_to_state(80.0f, ThermalState::COOL) == ThermalState::HOT);
    REQUIRE(temp_to_state(85.0f, ThermalState::COOL) == ThermalState::CRITICAL);
}
