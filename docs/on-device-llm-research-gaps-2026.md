# On-Device LLM: Research Landscape & Build Opportunities
**As of June 14, 2026**

---

## 1. The Problem Space

Running LLMs on consumer hardware (phones, laptops, desktops without GPUs) is bottlenecked by four hard constraints:

| Constraint | Current Reality |
|---|---|
| **RAM** | Mobile: 4–12 GB shared with OS. 7B model at FP16 = 14 GB — impossible without compression |
| **Storage** | Users won't install a 10 GB model. Target: <2 GB |
| **Battery** | A 7B INT4 model burns ~0.7 J/token. ~490–590 prompts drains iPhone 14 Pro fully |
| **CPU/NPU Throughput** | 3B models need 3–10 GFLOPS/token; mobile CPU tops at ~4 GFLOPS sustained before throttling |

A model that works in a lab benchmark but triggers thermal throttling after 15 minutes or drains 40% battery per conversation is not a real product.

---

## 2. What Has Been Solved (or is close)

### 2.1 Quantization — Largely Solved for Static Inference

The field has moved fast here:

- **INT8/INT4 PTQ** (Post-Training Quantization): mainstream, shipped in llama.cpp, GGUF format, ExecuTorch
- **GPTQ, AWQ, SmoothQuant**: reduce accuracy loss at 4-bit; widely deployed
- **FP8**: most stable option on NVIDIA Hopper (8.5% latency drop, 33% throughput gain over FP16) — server side
- **BitNet b1.58** (Microsoft, April 2025): first open-source **1-bit** (ternary) 2B model, weights fit in 0.4 GB. 2.37–6.17x speedup on x86 CPU, 2.71x faster with 3.55x less RAM than FP16
- **Dynamic quantization** (FlexQuant, June 2025): layer-wise mixed-precision switching per token based on perplexity entropy — 1.3x end-to-end speedup with negligible accuracy loss
- **MoBiQuant** (Feb 2026): token-adaptive elastic quantization, mixture-of-bits per token

**What's still not solved**: adaptive quantization that responds to *device thermal state and battery level* in real time — not just to model perplexity.

---

### 2.2 Model Architecture Compression

- **Pruning**: structured/unstructured removal of redundant weights. Research shows P→KD→Q ordering (Pruning, then Knowledge Distillation, then Quantization) gives best balance
- **Knowledge Distillation**: large teacher → small student. Microsoft Phi series uses synthetic datasets. A 2025 study compressed to 250 MB with 85% teacher accuracy, halving training time
- **Mixture-of-Experts (MoE)**: sparse activation — only 2/N experts activate per token. Cuts compute but memory still loads all experts. **Partial solve.**
- **Apple on-device model** (July 2025): ~3B params with 2-bit QAT (quantization-aware training), ships on iOS 19 devices
- **Google Gemma 3 270M** (Aug 2025): 0.75% battery for 25 conversations on Pixel 9 Pro with INT4

---

### 2.3 Inference Runtimes — Mature, Fragmented

| Tool | Type | Focus | Notes |
|---|---|---|---|
| **llama.cpp** | Open source | CPU-first, x86/ARM | 91K+ stars, GGUF format, widest quantization support |
| **MLC LLM** (Apache TVM) | Open source | GPU-first mobile | Metal, Vulkan, OpenCL, WebGPU backends; paged KV cache |
| **ExecuTorch** (Meta) | Open source, production | Mobile/edge PyTorch | v1.0 GA Oct 2025, 50KB footprint, 12+ backends, ships in Instagram/WhatsApp |
| **Ollama** | Open source | Desktop | Developer-friendly wrapper around llama.cpp |
| **MLX** (Apple) | Open source | Apple Silicon | M-series only, fast on Neural Engine |
| **TensorRT-LLM Edge** | Paid (NVIDIA) | NVIDIA hardware | Best for Jetson/desktop GPU |
| **Qualcomm AI Hub** | Paid | Snapdragon NPU | Optimizes models for Snapdragon 8 Elite (45 TOPS) and X2 Extreme (80 TOPS) |
| **MNN-AECS** (Alibaba) | Research/partial OSS | Mobile energy | Adaptive Core Selection for power-aware decoding |

**Key gap**: No runtime unifies NPU + CPU + GPU routing *dynamically* based on workload, thermal state, and battery — all runtimes target one primary backend.

---

### 2.4 Flash/NAND Memory Offloading

When DRAM runs out, models can partially live on storage:

- **KVNAND** (Dec 2025): first DRAM-free architecture — model weights AND KV cache in compute-enabled 3D NAND flash. Eliminates DRAM bottleneck for long contexts
- **Active Weight Swapping** (April 2025): cross-layer preloading + sparsity-aware distillation + pipeline-aware swapping between DRAM and flash
- **NVLLM** (2026): 3D NAND-centric design — FFN weights in flash, attention weights in DRAM

**Still open**: software-level abstraction that makes flash-offloading transparent to app developers, with automatic tiering decisions.

---

### 2.5 Speculative Decoding

Using a small draft model to propose N tokens that the large model verifies in parallel:
- **2–3x speedup** in production (NVIDIA, vLLM, TensorRT-LLM)
- DART (Jan 2026): reduces drafting forward latency by 6.8x vs EAGLE3
- CPU/GPU heterogeneous speculative decoding for consumer devices (EMNLP 2025)

**Gap**: No production-grade speculative decoding for mobile that auto-selects the draft model based on the target model in use and device capability.

---

### 2.6 Distributed/Collaborative Inference

- **PETALS**: P2P inference of 100B+ models across internet nodes with dynamic quantization and load balancing
- **PPAI** (2026): first personalized LLM agent interoperability — P2P task delegation based on agent specialization
- **Splitwise** (2025): Lyapunov-assisted DRL for hybrid edge-cloud inference optimization
- **Edge partitioning research**: linear programming to split model layers across heterogeneous devices

**Gap**: No consumer-grade app that lets two phones or a phone + laptop collaboratively run a single model with zero configuration.

---

## 3. Existing Paid Products

| Product | Company | What It Does | Price Model |
|---|---|---|---|
| **Qualcomm AI Hub** | Qualcomm | Optimize and deploy models for Snapdragon NPU | Enterprise/per-device |
| **TensorRT-LLM Edge** | NVIDIA | Edge inference on Jetson + RTX | License + hardware |
| **Core ML + ANE tooling** | Apple | On-device for Apple Silicon (private APIs) | Ecosystem-locked |
| **Samsung AI tooling** | Samsung | Gauss models on Galaxy devices (proprietary) | OEM-only |
| **Google AI Edge** | Google | Gemma + Mediapipe on Android/Pixel | Free tier + enterprise |
| **Intel OpenVINO** | Intel | Core Ultra NPU inference | Free + paid support |

Most paid products are either **hardware-vendor locked** or require **enterprise agreements**. No neutral cross-hardware paid runtime exists.

---

## 4. Core Unsolved Problems (As of June 14, 2026)

### P1 — Thermal Awareness at the Runtime Layer
**What exists**: Models are compressed offline. Runtime has no live feedback loop.  
**What's missing**: A runtime that monitors device core temp and clock frequency, and dynamically adjusts:
- Which layers run on CPU vs NPU vs GPU
- Quantization bit-width per layer, per token
- Whether to defer inference to a cooler moment

iPhone 16 Pro drops 41.5% throughput under sustained load due to throttling. No tool addresses this adaptively.

---

### P2 — Battery-Budget Inference
**What exists**: Some energy papers (MNN-AECS) show core selection helps. Gemma 270M achieves low battery use by being tiny.  
**What's missing**: An inference scheduler that accepts a battery budget ("I want to spend max 2% battery on this response") and dynamically selects model size, quantization level, and inference path to hit that budget.

---

### P3 — Automatic Cross-Hardware NPU Routing
**What exists**: Each runtime targets one backend. Qualcomm tools work on Snapdragon, MLX on Apple, OpenVINO on Intel.  
**What's missing**: A hardware-agnostic dispatcher that detects available accelerators (NPU type, TOPS rating, driver version) and compiles/routes accordingly — like a "LLVM for LLM inference."

---

### P4 — On-Device Continual Personalization Without Full Retraining
**What exists**: LoRA fine-tuning (offline). RAG (stateless retrieval). Static model weights.  
**What's missing**: A mechanism for the on-device model to incrementally learn user preferences, writing style, and domain knowledge across sessions — without retraining from scratch and without sending data to the cloud. Research shows models drop to 40–60% accuracy on multi-session agentic tasks vs. near-perfect passive recall.

---

### P5 — Cross-Session Contextual Memory with Staleness Detection
**What exists**: Vector DBs, RAG pipelines. mem0, MemGPT-style memory for cloud agents.  
**What's missing**: On-device memory system that:
1. Detects when a stored fact is stale (e.g., user changed jobs)
2. Resolves identity across sessions (anonymous → returning user)
3. Works entirely offline within the device's storage/compute budget

---

### P6 — Zero-Config P2P Collaborative Inference
**What exists**: PETALS (requires stable internet + technical setup). Research prototypes.  
**What's missing**: A consumer app that:
- Discovers nearby devices over WiFi/Bluetooth
- Partitions a larger model automatically across them
- Lets a phone + laptop together run a 13B model neither could run alone

---

### P7 — Model-to-Model Compression Pipeline (Automated)
**What exists**: Individual techniques (pruning, KD, quantization) with manual tuning.  
**What's missing**: An automated pipeline that takes any HuggingFace model and produces a device-specific compressed artifact through P→KD→Q with hardware-in-the-loop profiling. Qualcomm AI Hub does part of this but only for Snapdragon.

---

### P8 — Flash-Transparent Inference SDK
**What exists**: KVNAND, active weight swapping — all research-grade, no SDK.  
**What's missing**: A developer SDK where you specify "model X, device Y" and it transparently manages DRAM/flash tiering, prefetch scheduling, and KV cache placement without the developer knowing the storage architecture.

---

### P9 — LLM Inference Profiler for Developers
**What exists**: GPU profilers (Nsight, Instruments). llama.cpp prints tokens/sec.  
**What's missing**: A cross-device profiler that shows:
- Per-layer latency, memory bandwidth, power draw
- Bottleneck identification (memory-bound vs compute-bound per layer)
- Recommendation engine: "change layer 12 quant from Q4 to Q6 to unblock this bottleneck"

---

### P10 — Unified Model Format with Hardware Metadata
**What exists**: GGUF (llama.cpp), SafeTensors (HuggingFace), ExecuTorch .pte, CoreML .mlpackage — all incompatible.  
**What's missing**: A single portable model container that embeds hardware-specific compilation artifacts, quantization manifests, and fallback chains — so one file runs optimally on any device.

---

## 5. What Can Be Built (Hasn't Existed as of June 14, 2026)

These are concrete product opportunities, ordered by feasibility:

---

### B1 — Thermal-Aware Inference Daemon (High Feasibility)
**What**: A background OS service (Linux/Android/macOS/Windows) that sits between the app and llama.cpp/ExecuTorch, monitoring:
- CPU/GPU/NPU temp via sysfs/IOKit/WMI
- Battery SoC and discharge rate
- Clock throttling events

It dynamically swaps quantization presets mid-inference (Q8 → Q4 → Q2) and shifts layer routing between CPU cores and NPU.

**Why it hasn't been built**: Runtime teams optimize for peak perf. No one owns the cross-layer "thermal budget" problem because it spans OS, hardware driver, and inference runtime.

**Stack**: Rust daemon + llama.cpp FFI + platform thermal APIs

---

### B2 — Battery-Budget Inference API (High Feasibility)
**What**: A local API layer where callers specify `max_battery_pct: 1.5` and the system auto-selects the largest model + quantization level that fits within that budget based on learned per-device energy profiles.

**Novel aspect**: Learns energy-per-token for each model/quant combo on the specific device at first run, stores a calibration profile, and uses it for future scheduling.

---

### B3 — On-Device Incremental LoRA Adapter (Medium Feasibility)
**What**: A framework for continuously updating a LoRA adapter from user interactions, stored locally, applied at inference time. The base model never changes. Only the 1–5 MB adapter evolves.

**Key challenge**: Catastrophic forgetting in LoRA updates → needs elastic weight consolidation or replay buffer.

**Moat**: Privacy-first (no data leaves device), works offline, no cloud fine-tuning cost.

---

### B4 — Cross-Device Mesh Inference App (Medium Feasibility)
**What**: Mobile/desktop app with a single-tap "mesh mode." Discovers devices on LAN, negotiates layer partitioning for a target model, runs pipeline-parallel inference. Fallback to solo inference if peers drop.

**Novel aspect**: Consumer UX. All existing distributed inference requires CLI setup, static configs, and reliable internet. This targets families, small teams, hobbyists.

**Stack**: Rust + llama.cpp pipeline parallel + mDNS discovery + QUIC transport

---

### B5 — Automated Model Compression Pipeline (Medium Feasibility)
**What**: Web app / CLI: input a HuggingFace model ID + target device profile (Snapdragon 8 Elite, Apple M3, Intel Core Ultra 9, generic x86) → outputs a compressed artifact with benchmark report.

Pipeline: P→KD→Q with hardware-in-the-loop benchmarking using device emulation or real device farm.

**Revenue model**: Free for <3B models. Paid for larger models or private model uploads.

---

### B6 — Cross-Device Model Format (Low Feasibility / High Impact)
**What**: A model container spec (like GGUF but with multi-backend compilation artifacts) that embeds:
- Quantization manifests per hardware class
- Precompiled kernels for top 10 device profiles
- Fallback chain (NPU → GPU → CPU)
- Hardware detection at load time

**Why hard**: Requires buy-in from llama.cpp, ExecuTorch, MLC LLM communities — a standards problem, not just an engineering problem.

---

### B7 — On-Device LLM Inference Profiler (High Feasibility)
**What**: A GUI tool (cross-platform Electron or Tauri app) that:
1. Loads any GGUF model
2. Runs a calibration suite
3. Shows per-layer latency/bandwidth/power heatmap
4. Gives actionable recommendations

**Market**: LLM developers who want to optimize for edge but have no visibility into what's slow and why.

---

### B8 — Staleness-Aware On-Device Memory System (High Research, Medium Product)
**What**: A local memory store for LLM conversations that:
- Tags facts with confidence decay curves (recent facts more trusted)
- Detects contradictions between new input and old memory (e.g., "I work at X" after "I work at Y")
- Resolves silently or asks the user to confirm

**Stack**: SQLite + semantic similarity index (FAISS/usearch) + a small classifier for contradiction detection.

---

## 6. Summary Gap Matrix

| Gap | Research Exists | Tool Exists | Product Opportunity |
|---|---|---|---|
| Thermal-aware dynamic quant | Partial | No | **B1** |
| Battery-budget scheduling | Minimal | No | **B2** |
| On-device incremental learning | Papers only | No | **B3** |
| Consumer P2P mesh inference | Research (PETALS) | No consumer app | **B4** |
| Automated compression pipeline | Research (P→KD→Q) | Partial (Qualcomm-only) | **B5** |
| Unified cross-hardware model format | No | No | **B6** |
| Per-layer inference profiler | No | No | **B7** |
| Staleness-aware on-device memory | Papers only | No | **B8** |
| Flash-transparent inference SDK | KVNAND (paper) | No | Extension of B5 |
| Cross-hardware NPU dispatcher | OpenVINO (partial) | Intel-only | Extension of B1 |

---

## 7. Key Papers (2025–2026)

- **KVNAND** (Dec 2025): DRAM-free in-flash computing for LLM — arxiv 2512.03608
- **FlexQuant** (2025): Dynamic precision switching — arxiv 2506.12024
- **MoBiQuant** (Feb 2026): Token-adaptive mixture-of-bits — arxiv 2602.20191
- **DART** (Jan 2026): Diffusion-inspired speculative decoding — arxiv 2601.19278
- **MNN-AECS** (2026): Energy optimization via adaptive core selection — arxiv 2506.19884
- **NVLLM** (2026): 3D NAND-centric LLM architecture — arxiv 2604.25699
- **PPAI** (2026): P2P personalized agent interoperability — arxiv 2605.18067
- **Contextual Memory Intelligence** (2026): Reflective AI memory paradigm — arxiv 2506.05370
- **Active Weight Swapping** (April 2025): DRAM/flash swapping for large models — arxiv 2504.08378
- **Survey: Quantization in LLM** (2026): Comprehensive survey — Springer/JCST doi:10.1007/s11390-026-5979-1
- **Efficient LLMs for Edge Devices** (2025): Pruning/Quant/Distillation — IEEE 10968787

---

## 8. Hardware Landscape (June 2026)

| Chip | TOPS | Notable |
|---|---|---|
| Qualcomm Snapdragon X2 Extreme | 80 TOPS | Best mobile NPU as of Q1 2026 |
| Qualcomm Snapdragon 8 Elite | 45 TOPS | Flagship mobile |
| Apple M4 Neural Engine | 38 TOPS | Mac/iPad Pro |
| Intel Core Ultra (Lunar Lake) | ~40 TOPS | PC/laptop |
| Apple M5 (expected) | ~50 TOPS | Coming |

Even at 80 TOPS, a 7B INT4 model saturates NPU bandwidth in <2 seconds of sustained decoding before thermal limits hit. The hardware is there — the **software management layer** is not.

---

*Research compiled June 14, 2026. Sources: arxiv, IEEE, ACM, Meta AI, Apple ML Research, Octomil Docs, Edge AI Alliance.*
