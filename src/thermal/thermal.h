#pragma once
#include <cstdint>
#include <algorithm>

enum class ThermalState : uint8_t {
    COOL,      // < 70°C  — full speed
    WARM,      // 70–80°C — reduce threads
    HOT,       // 80–85°C — min threads
    CRITICAL   // > 85°C  — pause inference
};

struct InferenceParams {
    int  n_threads;
    int  n_batch;
    bool paused;
};

// Abstract platform thermal reader
class IThermalMonitor {
public:
    virtual ~IThermalMonitor() = default;
    virtual ThermalState read() = 0;
};

// Factory: returns the right impl for this platform (Linux/macOS/Windows)
IThermalMonitor* create_thermal_monitor();

// Shared conversion used by Linux + Windows (macOS uses OS-level states directly).
// Dead-bands prevent rapid oscillation at each threshold boundary.
inline ThermalState temp_to_state(float celsius, ThermalState prev) {
    ThermalState next;
    if      (celsius < 70.0f) next = ThermalState::COOL;
    else if (celsius < 80.0f) next = ThermalState::WARM;
    else if (celsius < 85.0f) next = ThermalState::HOT;
    else                       next = ThermalState::CRITICAL;

    // Dead-bands prevent rapid oscillation at each threshold boundary.
    // HOT→WARM: must drop below 78°C (not just 80°C) to exit HOT
    if (prev == ThermalState::HOT      && next == ThermalState::WARM && celsius >= 78.0f)
        next = ThermalState::HOT;
    // WARM→COOL: must drop below 68°C (not just 70°C) to exit WARM
    if (prev == ThermalState::WARM     && next == ThermalState::COOL && celsius >= 68.0f)
        next = ThermalState::WARM;
    // CRITICAL→HOT: must drop below 83°C (not just 85°C) to exit CRITICAL
    if (prev == ThermalState::CRITICAL && next == ThermalState::HOT  && celsius >= 83.0f)
        next = ThermalState::CRITICAL;

    return next;
}
