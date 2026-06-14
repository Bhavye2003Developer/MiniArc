#include "thermal.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

class LinuxThermalMonitor : public IThermalMonitor {
    ThermalState m_prev = ThermalState::COOL;

    static float max_cpu_temp() {
        float max_c = 0.0f;
        const fs::path base("/sys/class/thermal");
        if (!fs::exists(base)) return max_c;

        for (auto& zone : fs::directory_iterator(base)) {
            // Skip non-CPU zones
            auto type_path = zone.path() / "type";
            if (fs::exists(type_path)) {
                std::ifstream tf(type_path);
                std::string type; tf >> type;
                for (auto& skip : {"ambient", "battery", "fan", "pmic", "skin"}) {
                    if (type.find(skip) != std::string::npos) goto next_zone;
                }
            }
            {
                std::ifstream f(zone.path() / "temp");
                if (f) {
                    int millideg = 0;
                    f >> millideg;
                    max_c = std::max(max_c, millideg / 1000.0f);
                }
            }
            next_zone:;
        }
        return max_c;
    }

public:
    ThermalState read() override {
        float t = max_cpu_temp();
        if (t <= 0.0f) {
            // No readable zones — return previous state rather than silently reporting COOL
            return m_prev;
        }
        ThermalState next = temp_to_state(t, m_prev);
        m_prev = next;
        return next;
    }
};

IThermalMonitor* create_thermal_monitor() {
    return new LinuxThermalMonitor();
}
