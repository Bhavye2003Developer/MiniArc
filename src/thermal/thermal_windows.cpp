#include "thermal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include <powrprof.h>
#include <vector>
#include <algorithm>

// PROCESSOR_POWER_INFORMATION is in powrprof.h but may need manual declaration
// depending on the Windows SDK version.
#ifndef PROCESSOR_POWER_INFORMATION_DEFINED
typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#define PROCESSOR_POWER_INFORMATION_DEFINED
#endif

class WindowsThermalMonitor : public IThermalMonitor {
    IWbemLocator*  m_loc  = nullptr;
    IWbemServices* m_svc  = nullptr;
    bool           m_wmi  = false;
    ThermalState   m_prev = ThermalState::COOL;

    bool init_wmi() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IWbemLocator, reinterpret_cast<void**>(&m_loc));
        if (FAILED(hr)) return false;

        hr = m_loc->ConnectServer(_bstr_t(L"ROOT\\WMI"),
                                  nullptr, nullptr, nullptr, 0, nullptr, nullptr, &m_svc);
        if (FAILED(hr)) return false;

        hr = CoSetProxyBlanket(m_svc,
                               RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                               RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                               nullptr, EOAC_NONE);
        return SUCCEEDED(hr);
    }

    // Returns temperature in °C, or -1.0f on failure
    float wmi_temp() {
        IEnumWbemClassObject* e = nullptr;
        HRESULT hr = m_svc->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &e);
        if (FAILED(hr)) return -1.0f;

        float max_c = 0.0f;
        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (e->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            VARIANT v;
            if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, nullptr, nullptr))) {
                // Tenths of Kelvin → Celsius
                float c = static_cast<float>(v.uintVal) / 10.0f - 273.15f;
                max_c = std::max(max_c, c);
                VariantClear(&v);
            }
            obj->Release();
        }
        e->Release();
        return (max_c > 0.0f) ? max_c : -1.0f;
    }

    // Fallback: use processor clock ratio via CallNtPowerInformation
    ThermalState clock_ratio_state() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD n = si.dwNumberOfProcessors;
        std::vector<PROCESSOR_POWER_INFORMATION> ppi(n);
        if (CallNtPowerInformation(ProcessorInformation, nullptr, 0,
                                   ppi.data(), n * sizeof(PROCESSOR_POWER_INFORMATION)) != 0)
            return ThermalState::COOL;

        ULONG min_cur = ppi[0].CurrentMhz;
        ULONG max_mhz = ppi[0].MaxMhz;
        for (DWORD i = 1; i < n; ++i) {
            min_cur = std::min(min_cur, ppi[i].CurrentMhz);
            max_mhz = std::max(max_mhz, ppi[i].MaxMhz);
        }
        if (max_mhz == 0) return ThermalState::COOL;
        double ratio = static_cast<double>(min_cur) / max_mhz;

        if (ratio >= 0.85) return ThermalState::COOL;
        if (ratio >= 0.75) return ThermalState::WARM;
        if (ratio >= 0.60) return ThermalState::HOT;
        return ThermalState::CRITICAL;
    }

public:
    WindowsThermalMonitor() { m_wmi = init_wmi(); }

    ~WindowsThermalMonitor() {
        if (m_svc) m_svc->Release();
        if (m_loc) m_loc->Release();
    }

    ThermalState read() override {
        ThermalState next;
        if (m_wmi) {
            float t = wmi_temp();
            if (t < 0.0f) {
                next = clock_ratio_state();
            } else {
                next = temp_to_state(t, m_prev);
            }
        } else {
            next = clock_ratio_state();
        }
        m_prev = next;
        return next;
    }
};

IThermalMonitor* create_thermal_monitor() {
    return new WindowsThermalMonitor();
}
