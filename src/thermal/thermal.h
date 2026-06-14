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
// Hysteretic: once HOT, must drop below 78°C to return to WARM.
inline ThermalState temp_to_state(float celsius, ThermalState prev) {
    ThermalState next;
    if      (celsius < 70.0f) next = ThermalState::COOL;
    else if (celsius < 80.0f) next = ThermalState::WARM;
    else if (celsius < 85.0f) next = ThermalState::HOT;
    else                       next = ThermalState::CRITICAL;

    // Hysteresis prevents oscillation between WARM and HOT around the 80°C mark
    if (prev == ThermalState::HOT && next == ThermalState::WARM && celsius >= 78.0f)
        next = ThermalState::HOT;

    return next;
}
