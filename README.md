# miniARC

A thermal-aware offline LLM chat CLI. One binary, one model file, no internet required. Runs a quantized language model entirely on-device and automatically throttles inference when your CPU gets hot.

Named after Iron Man's arc reactor — miniaturized, self-sufficient power.

---

## What it does

- Runs **Qwen 2.5 0.5B** (Q4_K_M, ~469 MB) fully on-device via [llama.cpp](https://github.com/ggerganov/llama.cpp)
- Two modes: **terminal CLI** and **local web UI** (`--serve`)
- Streams tokens in real-time in both modes
- Monitors CPU temperature every 2 seconds in a background thread
- Automatically reduces thread count or pauses inference when the device gets hot
- Resumes at full speed when it cools down
- Supports swapping to any other GGUF model at runtime

---

## Requirements

| Platform | Requirements |
|---|---|
| Windows 10+ | CMake ≥ 3.21, Visual Studio 2022 with C++ workload |
| Linux (Ubuntu 22.04+) | CMake ≥ 3.21, GCC ≥ 11 or Clang ≥ 14, `build-essential` |
| macOS 13+ | CMake ≥ 3.21, Xcode Command Line Tools |

Hardware: any x86_64 or arm64 machine with ≥ 2 GB RAM free. No GPU required.

---

## Installation

### 1. Clone the repo

```bash
git clone https://github.com/Bhavye2003Developer/MiniArc.git
cd MiniArc
```

### 2. Download the model

**Windows (PowerShell):**
```powershell
.\scripts\download_model.ps1
```

**Linux / macOS:**
```bash
./scripts/download_model.sh
```

This downloads `qwen2.5-0.5b-instruct-q4_k_m.gguf` (~469 MB) from HuggingFace into the `models/` directory.

### 3. Configure and build

**Windows:**
```powershell
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The first build fetches llama.cpp (b5891) and Catch2 automatically via CMake FetchContent — no manual submodule steps needed.

### 4. Run

**Windows:**
```powershell
.\build\Release\miniARC.exe
```

**Linux / macOS:**
```bash
./build/miniARC
```

If you have multiple GGUF files, pass the path explicitly:
```bash
./build/Release/miniARC.exe models\qwen2.5-0.5b-instruct-q4_k_m.gguf
```

---

## Usage

### Terminal (CLI mode)

```
┌─────────────────────────────────────────────────────┐
│  miniARC v0.1.0  ·  qwen2.5-0.5b-instruct-q4_k_m.gguf  │
│  ● COOL  ·  RAM: 162 MB  ·  0.0 tok/s              │
└─────────────────────────────────────────────────────┘
Type /help for commands. Ctrl+C to quit.

[14:32:01] You: what is the capital of France?
miniARC: The capital of France is Paris. It is a major European city and
a global centre for art, fashion, gastronomy, and culture.
(3.2s  ·  11.4 tok/s)

[14:32:08] You:
```

Each prompt is stamped with the current time. After every response, miniARC prints how long generation took and the tokens-per-second rate.

### Web UI (serve mode)

Launch the local web server:

```powershell
# Windows
.\build\Release\miniARC.exe --serve

# Linux / macOS
./build/miniARC --serve
```

The browser opens automatically at `http://localhost:7860`. Use `--port` to pick a different port:

```powershell
.\build\Release\miniARC.exe --serve --port 8888
```

**Web UI features:**
- Real-time token streaming as the model generates
- Dark, responsive interface
- Status bar showing thermal state, threads, RAM, and live tokens/sec
- "Clear conversation" button
- No internet required — everything runs locally

### Commands

| Command | Description |
|---|---|
| `/status` | Show live thermal state, threads, batch size, tokens/sec, RAM |
| `/clear` | Reset conversation history (model stays loaded) |
| `/model <path>` | Swap to a different GGUF model file at runtime |
| `/threads <n>` | Override thread count manually (0 restores automatic) |
| `/help` | List all commands |
| `/quit` or `/exit` | Exit cleanly |
| `Ctrl+C` | Exit cleanly |

### Thermal notifications

When temperature changes, miniARC prints inline between responses:

```
[◐ WARM — reducing to 4 threads]
[○ HOT — throttling to 2 threads]
[✕ CRITICAL — pausing inference until cool]
[● COOL — full speed resumed]
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│   CLI loop · Command parser · Banner · Token streaming      │
│   --serve → WebServer · auto-open browser                   │
└────────────┬───────────────────────────┬────────────────────┘
             │ CLI mode                  │ --serve mode
             │                  ┌────────▼────────┐
             │                  │   WebServer     │
             │                  │   server.cpp    │
             │                  │   cpp-httplib   │
             │                  │   SSE streaming │
             │                  └────────┬────────┘
             │ calls                     │ calls
          ┌──▼────────────────────────────▼──────┐
          │   Engine                              │◄──── InferenceParams
          │   engine.cpp                          │           │
          │                                       │           │ reads every 32 tokens
          │  ┌───────────────┐                    │    ┌──────┴──────┐
          │  │  llama.cpp    │                    │    │  Scheduler  │
          │  │  (b5891)      │                    │    │  scheduler.cpp│
          │  │  GGUF model   │                    │    └──────┬──────┘
          │  └───────────────┘                    │           │ update(state)
          └───────────────────────────────────────┘    ┌──────┴──────────┐
                                                        │ ThermalMonitor  │
                                                        │ (background     │
                                                        │  thread, 2s)    │
                                                        └──────┬──────────┘
                                                               │ read()
                                                      ┌────────┴────────┐
                                                      │ Platform impl   │
                                                      │ Linux: sysfs    │
                                                      │ macOS: IOKit    │
                                                      │ Windows: WMI    │
                                                      └─────────────────┘
```

### Directory layout

```
miniARC/
├── CMakeLists.txt                  Top-level build
├── src/
│   ├── main.cpp                    Entry point: CLI loop, --serve flag, arg parsing
│   ├── engine/
│   │   ├── engine.h
│   │   └── engine.cpp              llama.cpp wrapper: load, generate, history
│   ├── scheduler/
│   │   ├── scheduler.h
│   │   └── scheduler.cpp           Maps ThermalState → InferenceParams
│   ├── server/
│   │   ├── server.h                WebServer class declaration
│   │   └── server.cpp              HTTP routes, SSE streaming, embedded HTML UI
│   └── thermal/
│       ├── thermal.h               ThermalState enum, IThermalMonitor interface,
│       │                           temp_to_state() with hysteresis
│       ├── thermal_monitor.h/.cpp  Background polling thread, change callbacks
│       ├── thermal_linux.cpp       sysfs /sys/class/thermal reader
│       ├── thermal_windows.cpp     WMI + clock-ratio fallback
│       └── thermal_macos.mm        IOKit SMC reader (Objective-C++)
├── tests/
│   ├── CMakeLists.txt
│   ├── test_thermal_logic.cpp      Unit tests: hysteresis, state transitions
│   └── test_scheduler.cpp          Unit tests: thread/batch mapping
├── scripts/
│   ├── download_model.sh           Linux/macOS model downloader
│   └── download_model.ps1          Windows model downloader
└── models/
    └── .gitkeep                    GGUF files go here (gitignored)
```

---

## How it works

### Token generation pipeline

Each turn goes through three stages:

**1. Prompt formatting**

The engine formats the conversation as a ChatML prompt using the model's built-in chat template:

```
<|im_start|>system
You are miniARC, a concise and helpful AI assistant running entirely on-device...
<|im_end|>
<|im_start|>user
what is 2+2?
<|im_end|>
<|im_start|>assistant
```

If the accumulated history would exceed the 2048-token context window, the oldest user+assistant turn pair is dropped until it fits. The system prompt is never truncated.

**2. Prompt encoding**

The full formatted prompt is tokenized and fed to llama.cpp in a single `llama_decode` call. Only the last token's logits are requested (everything else is masked) to avoid wasting compute.

**3. Autoregressive generation**

Tokens are sampled one at a time using this sampler chain:

```
raw logits
    → top-k(40)           restrict to 40 most likely tokens
    → top-p(0.95)         nucleus sampling within that set
    → penalties(64, 1.15) penalize tokens seen in last 64 positions
    → temperature(0.7)    scale probability distribution
    → dist()              sample from final distribution
```

Each generated token is converted to a UTF-8 string fragment and streamed to the callback immediately. A 13-character sliding window buffers pieces to detect end-of-turn markers (`<|im_end|>`, `<|endoftext|>`) that span multiple BPE tokens before they reach the terminal.

### Thermal system

A background thread polls CPU temperature every 2 seconds and maps it to one of four states:

| State | Temperature | Threads | Batch | Effect |
|---|---|---|---|---|
| COOL | < 70°C | all cores | 512 | Full speed |
| WARM | 70–80°C | half cores | 256 | Reduced throughput |
| HOT | 80–85°C | 2 | 128 | Heavy throttle |
| CRITICAL | > 85°C | — | — | Inference paused |

**Hysteresis** prevents oscillation at boundaries. Once HOT, the temperature must drop below 78°C (not 80°C) to return to WARM. A fast drop from HOT directly into COOL territory steps through WARM first if temperature is still ≥ 68°C.

The generation loop reads scheduler params every 32 tokens (not every token) to avoid a mutex acquisition on each sample.

**Platform temperature sources:**

- **Linux**: reads all `/sys/class/thermal/thermal_zone*/temp` files, takes the maximum, skips ambient/battery/fan/pmic/skin zones
- **Windows**: WMI query `SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature` (tenths of kelvin → °C), falls back to CPU clock-ratio heuristic if WMI is unavailable
- **macOS**: IOKit SMC keys `TC0P` / `TC0D`

**Windows COM threading**: WMI is initialized lazily on the poll thread (COM apartment requirement). An `on_thread_exit()` hook calls `CoUninitialize()` on the same thread before it exits.

### Context management

`n_ctx = 2048` tokens. `n_batch = n_ctx` so the entire prompt always fits in a single decode call regardless of conversation length. When history grows too long, turn pairs are dropped from the front until the prompt fits with at least 256 tokens of headroom for the response.

---

## Running tests

```bash
# Run all unit tests
cmake --build build --config Release --target test_miniARC
./build/tests/Release/test_miniARC.exe      # Windows
./build/tests/test_miniARC                  # Linux/macOS

# Or via CTest
cd build && ctest -C Release --output-on-failure
```

Tests cover:
- `temp_to_state()` for all state transitions and dead-band hysteresis cases
- Scheduler thread/batch mapping for each thermal state
- Manual thread override and resume behaviour

---

## Swapping models

Any GGUF model works. At startup:

```bash
./build/Release/miniARC.exe models/llama-3.2-1b-instruct-q4_k_m.gguf
```

Or swap at runtime without restarting:

```
You: /model models/phi-3-mini-4k-instruct-q4.gguf
[Model loaded: phi-3-mini-4k-instruct-q4.gguf]
```

The conversation history is preserved across model swaps.

---

## Performance

Tested on a 4-core laptop CPU (no GPU):

| Model | Quantization | Size | Speed |
|---|---|---|---|
| Qwen 2.5 0.5B | Q4_K_M | 469 MB | ~10–25 tok/s |

Speed depends on core count and thermal state. The thermal throttle will reduce it on hot hardware. Use `/status` to see live tokens/sec.

---

## Phase 2 roadmap

- **[done]** Web UI: `miniARC --serve` — dark-themed local web app with SSE token streaming
- GPU backend: CUDA / Metal via llama.cpp's existing GPU support
- OpenAI-compatible REST API: `/v1/chat/completions` endpoint for tool integration
- Android: JNI wrapper → AAR library
- iOS: Swift package wrapping the C++ core via Objective-C bridge
