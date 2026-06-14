#include "engine/engine.h"
#include "scheduler/scheduler.h"
#include "thermal/thermal_monitor.h"

#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace fs = std::filesystem;

// ── Globals for signal handler ───────────────────────────────────────────────
static volatile bool g_interrupted = false;
static void on_sigint(int) { g_interrupted = true; }

// ── Helpers ──────────────────────────────────────────────────────────────────

static const char* state_label(ThermalState s) {
    switch (s) {
        case ThermalState::COOL:     return "COOL";
        case ThermalState::WARM:     return "WARM";
        case ThermalState::HOT:      return "HOT ";
        case ThermalState::CRITICAL: return "CRIT";
    }
    return "????";
}

static const char* state_dot(ThermalState s) {
    switch (s) {
        case ThermalState::COOL:     return "●";
        case ThermalState::WARM:     return "◐";
        case ThermalState::HOT:      return "○";
        case ThermalState::CRITICAL: return "✕";
    }
    return "?";
}

static void print_banner(Engine& engine, ThermalState ts) {
    std::cout << "\n";
    std::cout << "┌─────────────────────────────────────────────────────┐\n";
    std::cout << "│  miniARC v0.1.0  ·  " << engine.current_model_name() << "              │\n";
    std::cout << "│  "
              << state_dot(ts) << " " << state_label(ts)
              << "  ·  RAM: " << engine.ram_usage_mb() << " MB"
              << "  ·  " << std::fixed << std::setprecision(1)
              << engine.last_tokens_per_sec() << " tok/s"
              << "              │\n";
    std::cout << "└─────────────────────────────────────────────────────┘\n";
    std::cout << "Type /help for commands. Ctrl+C to quit.\n\n";
}

static void print_help() {
    std::cout <<
        "Commands:\n"
        "  /status           Live thermal, threads, RAM, speed\n"
        "  /clear            Reset conversation history\n"
        "  /model <path>     Swap to a different GGUF file\n"
        "  /threads <n>      Override thread count (0 = auto)\n"
        "  /help             Show this message\n"
        "  Ctrl+C            Quit\n\n";
}

static void print_status(Engine& engine, Scheduler& scheduler, ThermalState ts) {
    auto p = scheduler.current_params();
    std::cout
        << "  Thermal : " << state_label(ts) << "\n"
        << "  Threads : " << p.n_threads << "\n"
        << "  Batch   : " << p.n_batch << "\n"
        << "  Paused  : " << (p.paused ? "yes" : "no") << "\n"
        << "  Speed   : " << std::fixed << std::setprecision(1)
        << engine.last_tokens_per_sec() << " tok/s\n"
        << "  RAM     : " << engine.ram_usage_mb() << " MB\n\n";
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // Determine model path
    std::string model_path;
    if (argc >= 2) {
        model_path = argv[1];
    } else {
        // Auto-discover first .gguf in models/
        const fs::path models_dir("models");
        if (fs::exists(models_dir)) {
            for (auto& entry : fs::directory_iterator(models_dir)) {
                if (entry.path().extension() == ".gguf") {
                    model_path = entry.path().string();
                    break;
                }
            }
        }
    }

    if (model_path.empty() || !fs::exists(model_path)) {
        std::cerr
            << "No model found in ./models/\n"
            << "Run:  ./scripts/download_model.sh      (Linux/macOS)\n"
            << "  or  .\\scripts\\download_model.ps1    (Windows)\n"
            << "  Downloads qwen2.5-0.5b-q4_k_m.gguf (~300 MB)\n";
        return 1;
    }

    std::signal(SIGINT, on_sigint);

    // Bootstrap
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    Scheduler scheduler(hw);

    // Thermal change callback: print inline notification
    auto on_thermal_change = [&](ThermalState old_s, ThermalState new_s) {
        std::cout << "\n";
        switch (new_s) {
            case ThermalState::WARM:
                std::cout << "[◐ WARM — reducing to "
                          << scheduler.current_params().n_threads << " threads]\n";
                break;
            case ThermalState::HOT:
                std::cout << "[○ HOT — throttling to 2 threads]\n";
                break;
            case ThermalState::CRITICAL:
                std::cout << "[✕ CRITICAL — pausing inference until cool]\n";
                break;
            case ThermalState::COOL:
                std::cout << "[● COOL — full speed resumed]\n";
                break;
        }
        std::cout << std::flush;
        (void)old_s;
    };

    ThermalMonitor thermal(scheduler, on_thermal_change);
    Engine engine(model_path);

    print_banner(engine, thermal.current());

    // ── Chat loop ────────────────────────────────────────────────────────────
    std::string line;
    while (!g_interrupted) {
        std::cout << "You: " << std::flush;

        if (!std::getline(std::cin, line)) break; // EOF
        if (g_interrupted) break;
        if (line.empty()) continue;

        // ── Commands ─────────────────────────────────────────────────────────
        if (line[0] == '/') {
            std::istringstream ss(line);
            std::string cmd; ss >> cmd;

            if (cmd == "/quit" || cmd == "/exit") break;

            if (cmd == "/help") {
                print_help();
            } else if (cmd == "/clear") {
                engine.clear_history();
                std::cout << "[History cleared]\n\n";
            } else if (cmd == "/status") {
                print_status(engine, scheduler, thermal.current());
            } else if (cmd == "/model") {
                std::string path; ss >> path;
                if (path.empty()) {
                    std::cout << "Usage: /model <path-to-file.gguf>\n\n";
                } else if (!engine.swap_model(path)) {
                    std::cout << "[Failed to load model: " << path << "]\n\n";
                } else {
                    std::cout << "[Model loaded: " << path << "]\n\n";
                }
            } else if (cmd == "/threads") {
                int n = 0; ss >> n;
                scheduler.override_threads(n);
                if (n == 0)
                    std::cout << "[Threads set to automatic]\n\n";
                else
                    std::cout << "[Threads locked to " << n << "]\n\n";
            } else {
                std::cout << "Unknown command. Type /help\n\n";
            }
            continue;
        }

        // ── Generate ─────────────────────────────────────────────────────────
        std::cout << "miniARC: " << std::flush;
        engine.generate(line, scheduler, [](const std::string& tok) {
            std::cout << tok << std::flush;
        });
        std::cout << "\n\n";
    }

    std::cout << "\n[miniARC] Goodbye.\n";
    return 0;
}
