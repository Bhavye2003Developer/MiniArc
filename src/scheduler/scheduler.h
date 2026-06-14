#pragma once
#include "thermal/thermal.h"
#include <mutex>

class Scheduler {
public:
    explicit Scheduler(int hw_threads);

    void update(ThermalState state);
    InferenceParams current_params() const;

    // Manual override: 0 = restore automatic
    void override_threads(int n);

    // Resume inference if paused (e.g., after manual /threads override)
    void resume();

private:
    int             m_hw;
    int             m_thread_override = 0; // 0 means no override
    InferenceParams m_params;
    mutable std::mutex m_mu;
};
