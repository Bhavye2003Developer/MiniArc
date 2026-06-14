#include <catch2/catch_test_macros.hpp>
#include "scheduler/scheduler.h"
#include <thread>

TEST_CASE("Scheduler: COOL gives full threads and batch 512") {
    int hw = (int)std::thread::hardware_concurrency();
    Scheduler s(hw);
    auto p = s.current_params();
    REQUIRE(p.n_threads == hw);
    REQUIRE(p.n_batch == 512);
    REQUIRE(p.paused == false);
}

TEST_CASE("Scheduler: WARM halves threads and sets batch 256") {
    Scheduler s(8);
    s.update(ThermalState::WARM);
    auto p = s.current_params();
    REQUIRE(p.n_threads == 4);
    REQUIRE(p.n_batch == 256);
    REQUIRE(p.paused == false);
}

TEST_CASE("Scheduler: HOT caps at 2 threads and batch 128") {
    Scheduler s(8);
    s.update(ThermalState::HOT);
    auto p = s.current_params();
    REQUIRE(p.n_threads == 2);
    REQUIRE(p.n_batch == 128);
    REQUIRE(p.paused == false);
}

TEST_CASE("Scheduler: CRITICAL sets paused=true with 1 thread and batch 64") {
    Scheduler s(8);
    s.update(ThermalState::CRITICAL);
    auto p = s.current_params();
    REQUIRE(p.paused == true);
    REQUIRE(p.n_threads == 1);
    REQUIRE(p.n_batch == 64);
}

TEST_CASE("Scheduler: recovers to COOL after CRITICAL") {
    Scheduler s(8);
    s.update(ThermalState::CRITICAL);
    s.update(ThermalState::COOL);
    auto p = s.current_params();
    REQUIRE(p.paused == false);
    REQUIRE(p.n_threads == 8);
    REQUIRE(p.n_batch == 512);
}

TEST_CASE("Scheduler: single-core HOT still has at least 1 thread") {
    Scheduler s(1);
    s.update(ThermalState::HOT);
    REQUIRE(s.current_params().n_threads >= 1);
}

TEST_CASE("Scheduler: WARM on 1-core gives at least 1 thread") {
    Scheduler s(1);
    s.update(ThermalState::WARM);
    REQUIRE(s.current_params().n_threads >= 1);
}

TEST_CASE("Scheduler: thread override takes effect") {
    Scheduler s(8);
    s.override_threads(3);
    REQUIRE(s.current_params().n_threads == 3);
}

TEST_CASE("Scheduler: thread override persists across thermal updates") {
    Scheduler s(8);
    s.override_threads(3);
    s.update(ThermalState::WARM);
    REQUIRE(s.current_params().n_threads == 3);
}

TEST_CASE("Scheduler: override_threads(0) restores automatic on next update") {
    Scheduler s(8);
    s.override_threads(3);
    s.override_threads(0);
    s.update(ThermalState::COOL);
    REQUIRE(s.current_params().n_threads == 8);
}

TEST_CASE("Scheduler: override_threads with positive n clears paused") {
    Scheduler s(8);
    s.update(ThermalState::CRITICAL);
    REQUIRE(s.current_params().paused == true);
    s.override_threads(2);
    REQUIRE(s.current_params().paused == false);
}

TEST_CASE("Scheduler: resume() clears paused") {
    Scheduler s(8);
    s.update(ThermalState::CRITICAL);
    REQUIRE(s.current_params().paused == true);
    s.resume();
    REQUIRE(s.current_params().paused == false);
}
