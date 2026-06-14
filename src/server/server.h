#pragma once
#include "engine/engine.h"
#include "scheduler/scheduler.h"
#include "thermal/thermal_monitor.h"
#include <mutex>
#include <string>

class WebServer {
public:
    WebServer(Engine& engine, Scheduler& scheduler, ThermalMonitor& thermal);
    void run(int port);   // blocks until the server stops

    static std::string json_str(const std::string& s);  // JSON-encode a string token

private:
    Engine&         m_engine;
    Scheduler&      m_scheduler;
    ThermalMonitor& m_thermal;
    std::mutex      m_gen_mutex;   // serialise concurrent chat requests
};
