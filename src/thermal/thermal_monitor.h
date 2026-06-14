#pragma once
#include "thermal.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

// Forward declaration to avoid circular include
class Scheduler;

// Runs IThermalMonitor::read() every 2 seconds on a background thread.
// On state change: updates Scheduler and calls the optional callback.
class ThermalMonitor {
public:
    using OnChange = std::function<void(ThermalState old_state, ThermalState new_state)>;

    explicit ThermalMonitor(Scheduler& scheduler, OnChange on_change = nullptr);
    ~ThermalMonitor();

    ThermalState current() const { return m_state.load(); }

    // Not copyable
    ThermalMonitor(const ThermalMonitor&) = delete;
    ThermalMonitor& operator=(const ThermalMonitor&) = delete;

private:
    void poll_loop();

    Scheduler&                       m_scheduler;
    OnChange                         m_on_change;
    std::unique_ptr<IThermalMonitor> m_impl;
    std::atomic<ThermalState>        m_state{ThermalState::COOL};
    std::atomic<bool>                m_running{true};
    std::thread                      m_thread;
};
