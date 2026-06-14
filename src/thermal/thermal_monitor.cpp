#include "thermal/thermal_monitor.h"
#include "scheduler/scheduler.h"
#include <chrono>

ThermalMonitor::ThermalMonitor(Scheduler& scheduler, OnChange on_change)
    : m_scheduler(scheduler)
    , m_on_change(std::move(on_change))
    , m_impl(create_thermal_monitor())
    , m_thread(&ThermalMonitor::poll_loop, this)
{}

ThermalMonitor::~ThermalMonitor() {
    {
        std::lock_guard<std::mutex> lk(m_cv_mu);
        m_running = false;
    }
    m_cv.notify_one();
    if (m_thread.joinable()) m_thread.join();
}

void ThermalMonitor::poll_loop() {
    while (m_running) {
        ThermalState new_state = m_impl->read();
        ThermalState old_state = m_state.exchange(new_state);

        if (new_state != old_state) {
            m_scheduler.update(new_state);
            if (m_on_change) m_on_change(old_state, new_state);
        }

        std::unique_lock<std::mutex> lk(m_cv_mu);
        m_cv.wait_for(lk, std::chrono::seconds(2), [this]{ return !m_running.load(); });
    }
}
