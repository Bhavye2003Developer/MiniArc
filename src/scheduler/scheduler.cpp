#include "scheduler/scheduler.h"
#include <algorithm>

Scheduler::Scheduler(int hw_threads)
    : m_hw(hw_threads)
    , m_params{hw_threads, 512, false}
{}

void Scheduler::update(ThermalState state) {
    std::lock_guard<std::mutex> lock(m_mu);
    switch (state) {
        case ThermalState::COOL:
            m_params = {m_hw, 512, false};
            break;
        case ThermalState::WARM:
            m_params = {std::max(1, m_hw / 2), 256, false};
            break;
        case ThermalState::HOT:
            m_params = {std::min(2, m_hw), 128, false};
            break;
        case ThermalState::CRITICAL:
            m_params = {1, 64, true};
            break;
    }
    // Apply manual thread override (0 = no override)
    if (m_thread_override > 0)
        m_params.n_threads = m_thread_override;
}

void Scheduler::override_threads(int n) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_thread_override = n;
    if (n > 0) m_params.n_threads = n;
}

InferenceParams Scheduler::current_params() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_params;
}
