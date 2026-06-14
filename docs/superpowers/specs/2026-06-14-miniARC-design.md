# miniARC — Design Spec
**Date**: 2026-06-14  
**Phase**: 1 — Desktop CLI (Linux / macOS / Windows)  
**Status**: Approved

---

## 1. What It Is

miniARC is a self-contained, thermal-aware LLM inference CLI. One binary, one GGUF model file, no installer, no runtime dependencies beyond libc. It runs a quantized general-purpose chat model entirely on-device and automatically throttles inference when the device gets hot.

Named after Iron Man's arc reactor — miniaturized, self-sufficient power.

---

## 2. Goals

- Model on disk: < 500 MB
- Model in RAM at runtime: < 512 MB
- Works on any x86_64 or arm64 Linux, macOS, or Windows machine with ≥ 2 GB RAM free
- Zero cloud calls — fully offline
- Responds to thermal pressure without user intervention
- Single binary + single model file = full installation

---

## 3. Non-Goals (Phase 1)

- Mobile (Android, iOS) — Phase 2
- GUI — Phase 2
- Networking / HTTP API — not in scope
- GPU inference — future; CPU only in Phase 1
- Fine-tuning or model training — not in scope

---

## 4. Model

**Selected model**: Qwen 2.5 0.5B — Q4_K_M quantization (GGUF)  
**Size on disk**: ~300 MB  
**RAM at runtime**: ~360 MB  
**Source**: HuggingFace `Qwen/Qwen2.5-0.5B-Instruct-GGUF`

Rationale: Best quality-to-size ratio at the sub-500MB constraint. Multilingual, strong instruction following for a 0.5B model. Q4_K_M strikes the best perplexity/speed balance for this size class.

Alternative model supported via `/model <path>` command — any GGUF file works.

---

## 5. Architecture

```
User input (stdin)
      │
      ▼
  CLI loop (main.cpp)
      │  reads prompt
      ▼
  Engine::generate(prompt, params)          ◄──── params from Scheduler
      │  calls llama.cpp C API
      │  streams tokens to stdout
      ▼
  (output printed token-by-token)

Background thread (every 2 seconds):
  ThermalMonitor::read_temp()
      │
      ▼
  ThermalState (COOL / WARM / HOT / CRITICAL)
      │
      ▼
  Scheduler::update(state)
      │  sets n_threads, n_batch, pause_flag
      ▼
  Engine reads scheduler params before each batch
```

### Directory Layout

```
miniARC/
├── CMakeLists.txt
├── third_party/
│   └── llama.cpp/                  ← git submodule (pinned release)
├── src/
│   ├── thermal/
│   │   ├── thermal.h               ← ThermalState enum, IThermalMonitor interface
│   │   ├── thermal_linux.cpp       ← sysfs impl
│   │   ├── thermal_macos.cpp       ← IOKit SMC impl
│   │   └── thermal_windows.cpp     ← WMI + clock-drift fallback impl
│   ├── scheduler/
│   │   ├── scheduler.h
│   │   └── scheduler.cpp           ← maps ThermalState → InferenceParams
│   ├── engine/
│   │   ├── engine.h
│   │   └── engine.cpp              ← llama.cpp wrapper, streaming
│   └── main.cpp                    ← CLI entry point, chat loop
├── scripts/
│   ├── download_model.sh           ← Linux/macOS: wget/curl from HuggingFace
│   └── download_model.ps1          ← Windows: Invoke-WebRequest
└── models/
    └── .gitkeep                    ← gitignored directory for GGUF files
```

---

## 6. Thermal System

### ThermalState

```cpp
enum class ThermalState { COOL, WARM, HOT, CRITICAL };
```

### Thresholds and Behavior

| State    | Temp Range | n_threads          | n_batch | Side Effect                                  |
|----------|------------|--------------------|---------|----------------------------------------------|
| COOL     | < 70°C     | hardware_concurrency | 512   | None                                         |
| WARM     | 70–80°C    | concurrency / 2    | 256     | Print `⚠ warm — reducing threads`           |
| HOT      | 80–85°C    | 2                  | 128     | Print `🔥 hot — slowing down`               |
| CRITICAL | > 85°C     | 0 (pause)          | —       | Pause after current token, wait until ≤WARM, resume |

State transitions are hysteretic: once HOT, must drop below 78°C to return to WARM (prevents rapid oscillation).

### Per-Platform Temperature Sources

**Linux**:  
Read all `/sys/class/thermal/thermal_zone*/temp` files. Take the maximum. Values are millidegrees Celsius — divide by 1000. Skip zones with type containing `ambient` or `battery`.

**macOS**:  
Query IOKit SMC for key `TC0P` (CPU proximity sensor). Fallback to `TC0D` (CPU die). If both unavailable, fall back to running `sysctl kern.cputhrottled` — a non-zero value indicates throttling, map to `HOT`.

**Windows**:  
Primary: WMI query `SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature`. Value is in tenths of kelvin — convert: `(val / 10) - 273.15`.  
Fallback (WMI unavailable): Measure baseline clock speed at startup via `QueryPerformanceFrequency`. Poll every 2 seconds. If current frequency is >15% below baseline, infer throttling and report `HOT`. If >25% below, report `CRITICAL`.

---

## 7. Scheduler

```cpp
struct InferenceParams {
    int n_threads;
    int n_batch;
    bool paused;
};

class Scheduler {
public:
    void update(ThermalState state);
    InferenceParams current_params() const;
};
```

Engine calls `scheduler.current_params()` before each decode batch. If `paused == true`, engine spins on a `std::condition_variable` until `paused` becomes false.

---

## 8. Engine

Wraps the llama.cpp C API (`llama.h`). Responsibilities:
- Load GGUF model from path; if load fails, print error and exit with code 1
- Maintain chat context (system prompt + history, up to `n_ctx = 2048` tokens)
- System prompt: `"You are miniARC, a concise and helpful AI assistant running entirely on-device. Keep responses clear and brief."`
- Stream tokens to stdout as they generate (no buffering)
- Accept `InferenceParams` before each batch
- Expose `swap_model(path)` for `/model` command (unloads, reloads, preserves history)

Context truncation: when history approaches `n_ctx`, drop the oldest user+assistant turn pair to make room. Never truncate the system prompt.

---

## 9. CLI

### Startup

```
┌──────────────────────────────────────────────────────┐
│  miniARC v0.1.0  ·  Qwen-2.5-0.5B-Q4               │
│  CPU: 52°C ● COOL  ·  RAM: 318 MB  ·  12.4 tok/s   │
└──────────────────────────────────────────────────────┘
Type /help for commands. Ctrl+C to quit.
```

### Chat loop

```
You: <input>
miniARC: <streamed response>
```

Thermal state changes print inline between responses (not mid-token):
```
[🔥 HOT — throttling to 2 threads]
[● COOL — full speed resumed]
```

### Commands

| Command | Behavior |
|---|---|
| `/status` | Print live thermal, threads, batch, tokens/sec, RAM usage |
| `/clear` | Reset conversation history, keep model loaded |
| `/model <path>` | Swap to a different GGUF file |
| `/threads <n>` | Override automatic thread count (0 restores auto) |
| `/help` | List commands |
| `Ctrl+C` | Flush current token, print newline, exit cleanly |

### First-run (no model found)

```
$ ./miniARC
No model found in ./models/
Run: ./scripts/download_model.sh
  Downloads qwen2.5-0.5b-q4_k_m.gguf (~300 MB) from HuggingFace
```

---

## 10. Build System

**Tool**: CMake ≥ 3.21 with C++17.

`CMakeLists.txt` adds `third_party/llama.cpp` as a subdirectory. Links `miniARC` against `llama` static library.

### Targets

| Platform | Binary name | Build command |
|---|---|---|
| Linux x86_64 | `miniARC` | `cmake -B build && cmake --build build -j` |
| macOS arm64 | `miniARC` | same |
| macOS x86_64 | `miniARC` | same |
| Windows x86_64 | `miniARC.exe` | `cmake -B build -G "Visual Studio 17 2022" && cmake --build build` |

Platform detection via CMake `CMAKE_SYSTEM_NAME`. Compiles the matching `thermal_<platform>.cpp` only.

### Windows IOKit alternative

Windows does not have IOKit. The `#ifdef _WIN32` guard in `thermal.h` selects `thermal_windows.cpp`. No cross-platform shim needed.

---

## 11. Distribution

- GitHub Releases with pre-built binaries for all 4 targets (via GitHub Actions)
- `download_model.sh` / `download_model.ps1` pull GGUF from HuggingFace on first run
- No installer. User puts binary in PATH or runs from directory.

---

## 12. Phase 2 Preview (Not In Scope Now)

- Android: JNI wrapper → AAR library
- iOS: Swift package wrapping the C++ core via Objective-C bridge
- HTTP API mode: `miniARC --serve :11434` with OpenAI-compatible `/v1/chat/completions`
- GPU backend: CUDA/Metal via llama.cpp's existing GPU support

---

## 13. Success Criteria (Phase 1)

- [ ] Runs on Ubuntu 22.04+, macOS 13+, Windows 10+ without any extra install
- [ ] Model loads in < 5 seconds on a modern laptop
- [ ] Generates ≥ 8 tokens/sec on a 4-core CPU at COOL state
- [ ] Thermal monitor detects temperature changes within 4 seconds
- [ ] At CRITICAL state, inference pauses and resumes correctly
- [ ] `/model`, `/clear`, `/status` commands all work
- [ ] Context truncation works without crash at long conversations
- [ ] Ctrl+C exits cleanly (no hanging threads)
