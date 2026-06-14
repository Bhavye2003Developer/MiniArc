#include "thermal.h"
#import <Foundation/Foundation.h>

// Uses NSProcessInfo.thermalState — public Apple API, no root required.
// Available macOS 10.10.3+. The OS handles hysteresis internally.
class MacOSThermalMonitor : public IThermalMonitor {
public:
    ThermalState read() override {
        switch ([[NSProcessInfo processInfo] thermalState]) {
            case NSProcessInfoThermalStateNominal:  return ThermalState::COOL;
            case NSProcessInfoThermalStateFair:     return ThermalState::WARM;
            case NSProcessInfoThermalStateSerious:  return ThermalState::HOT;
            case NSProcessInfoThermalStateCritical: return ThermalState::CRITICAL;
            default:                                return ThermalState::COOL;
        }
    }
};

IThermalMonitor* create_thermal_monitor() {
    return new MacOSThermalMonitor();
}
