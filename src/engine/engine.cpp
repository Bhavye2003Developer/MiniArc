#include "engine/engine.h"
#include "llama.h"
#include "ggml.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

static constexpr int MIN_GEN_TOKENS = 256;

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <sys/resource.h>
#endif

const std::string Engine::SYSTEM_PROMPT =
    "You are miniARC, a concise and helpful AI assistant running entirely "
    "on-device. Keep responses clear and brief.";

// ── Helpers ─────────────────────────────────────────────────────────────────

static size_t process_ram_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / (1024 * 1024);
    return 0;
#else
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss / (1024 * 1024);
#else
    return ru.ru_maxrss / 1024;
#endif
#endif
}

// ── Engine ───────────────────────────────────────────────────────────────────

Engine::Engine(const std::string& model_path, int n_ctx)
    : m_n_ctx(n_ctx)
{
    llama_backend_init();
    llama_log_set([](ggml_log_level, const char*, void*){}, nullptr);
    load(model_path);
}

Engine::~Engine() {
    unload();
    llama_backend_free();
}

void Engine::rebuild_sampler() {
    if (m_sampler) { llama_sampler_free(m_sampler); m_sampler = nullptr; }
    auto sparams = llama_sampler_chain_default_params();
    m_sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(m_config.top_k));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(m_config.top_p, 1));
    // Penalize after narrowing vocabulary (API note: avoid on full vocab).
    llama_sampler_chain_add(m_sampler, llama_sampler_init_penalties(64, m_config.repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(m_config.temperature));
    llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

void Engine::set_config(const ModelConfig& cfg) {
    m_config = cfg;
    rebuild_sampler();
}

void Engine::load(const std::string& path) {
    m_model_path = path;
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;

    m_model = llama_model_load_from_file(path.c_str(), mparams);
    if (!m_model) {
        std::cerr << "miniARC: failed to load model: " << path << "\n";
        std::exit(1);
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx     = m_n_ctx;
    cparams.n_batch   = m_n_ctx;
    cparams.n_threads = static_cast<int>(std::thread::hardware_concurrency());

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        std::cerr << "miniARC: failed to create context\n";
        std::exit(1);
    }

    rebuild_sampler();
}

void Engine::unload() {
    if (m_sampler) { llama_sampler_free(m_sampler); m_sampler = nullptr; }
    if (m_ctx)     { llama_free(m_ctx);              m_ctx     = nullptr; }
    if (m_model)   { llama_model_free(m_model);      m_model   = nullptr; }
}

bool Engine::swap_model(const std::string& new_path) {
    unload();
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;
    m_model = llama_model_load_from_file(new_path.c_str(), mparams);
    if (!m_model) {
        std::cerr << "miniARC: failed to load model: " << new_path << "\n";
        return false;
    }
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx     = m_n_ctx;
    cparams.n_batch   = m_n_ctx;
    cparams.n_threads = static_cast<int>(std::thread::hardware_concurrency());
    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        std::cerr << "miniARC: failed to create context\n";
        unload();
        return false;
    }
    rebuild_sampler();
    if (!m_sampler) {
        std::cerr << "miniARC: failed to init sampler chain\n";
        unload();
        return false;
    }
    m_model_path = new_path;
    return true;
}

std::string Engine::format_prompt(const std::string& user_input) {
    std::vector<llama_chat_message> msgs;
    msgs.push_back({"system", SYSTEM_PROMPT.c_str()});
    for (auto& t : m_history) {
        msgs.push_back({t.role.c_str(), t.content.c_str()});
    }
    msgs.push_back({"user", user_input.c_str()});

    const char* tmpl = llama_model_chat_template(m_model, nullptr);
    std::vector<char> buf(8192);
    int n = llama_chat_apply_template(tmpl,
                                      msgs.data(), msgs.size(),
                                      true,
                                      buf.data(), (int)buf.size());
    if (n < 0) {
        std::string out = "<|im_start|>system\n" + SYSTEM_PROMPT + "<|im_end|>\n";
        for (auto& t : m_history)
            out += "<|im_start|>" + t.role + "\n" + t.content + "<|im_end|>\n";
        out += "<|im_start|>user\n" + user_input + "<|im_end|>\n<|im_start|>assistant\n";
        return out;
    }
    if (n > (int)buf.size()) {
        buf.resize(n + 1);
        llama_chat_apply_template(tmpl,
                                  msgs.data(), msgs.size(), true,
                                  buf.data(), (int)buf.size());
    }
    return std::string(buf.data(), n);
}


void Engine::generate(const std::string& user_input,
                      Scheduler&         scheduler,
                      const TokenCB&     cb)
{
    std::vector<llama_token> prompt_tokens(m_n_ctx);
    int n_prompt = 0;
    for (int attempt = 0; attempt < 50; ++attempt) {
        std::string prompt = format_prompt(user_input);
        n_prompt = llama_tokenize(llama_model_get_vocab(m_model),
                                  prompt.c_str(), (int)prompt.size(),
                                  prompt_tokens.data(), (int)prompt_tokens.size(),
                                  true, false);
        if (n_prompt < 0) {
            std::cerr << "\nminiARC: tokenization failed\n";
            return;
        }
        if (n_prompt <= m_n_ctx - MIN_GEN_TOKENS) break;
        if (m_history.size() < 2) break;
        m_history.erase(m_history.begin(), m_history.begin() + 2);
    }
    if (n_prompt > m_n_ctx - MIN_GEN_TOKENS) {
        std::cerr << "\nminiARC: prompt too long to fit in context even after trimming\n";
        return;
    }
    prompt_tokens.resize(n_prompt);

    llama_memory_clear(llama_get_memory(m_ctx), false);
    llama_sampler_reset(m_sampler);

    {
        llama_batch batch = llama_batch_init(n_prompt, 0, 1);
        for (int i = 0; i < n_prompt; ++i) {
            batch.token[i]      = prompt_tokens[i];
            batch.pos[i]        = i;
            batch.n_seq_id[i]   = 1;
            if (batch.seq_id && batch.seq_id[i]) batch.seq_id[i][0] = 0;
            batch.logits[i]     = false;
        }
        batch.logits[n_prompt - 1] = true;
        batch.n_tokens = n_prompt;
        if (llama_decode(m_ctx, batch) != 0) {
            std::cerr << "\nminiARC: prompt decode failed\n";
            llama_batch_free(batch);
            return;
        }
        llama_batch_free(batch);
    }

    std::string response;
    std::string pending;
    static const std::string EOT = "<|im_end|>";
    static const std::string EOS = "<|endoftext|>";
    int pos = n_prompt;
    auto t_start = std::chrono::steady_clock::now();
    int n_generated = 0;

    // Determine generation limit from config (0 = fill remaining context).
    int max_gen = (m_config.max_new_tokens > 0) ? m_config.max_new_tokens : m_n_ctx;
    int max_pos = n_prompt + max_gen;
    if (max_pos > m_n_ctx) max_pos = m_n_ctx;

    InferenceParams p = scheduler.current_params();
    llama_set_n_threads(m_ctx, p.n_threads, p.n_threads);
    int thermal_tick = 0;

    while (pos < max_pos) {
        if (++thermal_tick >= 32) {
            thermal_tick = 0;
            InferenceParams np = scheduler.current_params();
            while (np.paused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                np = scheduler.current_params();
            }
            if (np.n_threads != p.n_threads) {
                llama_set_n_threads(m_ctx, np.n_threads, np.n_threads);
            }
            p = np;
        }

        llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);

        const llama_vocab* vocab = llama_model_get_vocab(m_model);
        if (llama_vocab_is_eog(vocab, tok)) break;

        char piece[256];
        int n = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, true);
        std::string text;
        if (n < 0) {
            std::vector<char> big((-n) + 1);
            n = llama_token_to_piece(vocab, tok, big.data(), (int)big.size(), 0, true);
            if (n > 0) text.assign(big.data(), n);
        } else if (n > 0) {
            text.assign(piece, n);
        }

        if (!text.empty()) {
            pending += text;

            const size_t WINDOW = 13;
            bool found_eot = (pending.find(EOT) != std::string::npos ||
                              pending.find(EOS) != std::string::npos);
            if (found_eot) {
                size_t cut = pending.find(EOT);
                size_t cut2 = pending.find(EOS);
                if (cut == std::string::npos) cut = cut2;
                else if (cut2 != std::string::npos) cut = (cut < cut2) ? cut : cut2;
                if (cut > 0) {
                    cb(pending.substr(0, cut));
                    response += pending.substr(0, cut);
                    ++n_generated;
                }
                pending.clear();
                break;
            }
            if (pending.size() > WINDOW) {
                size_t safe = pending.size() - (WINDOW - 1);
                cb(pending.substr(0, safe));
                response += pending.substr(0, safe);
                ++n_generated;
                pending = pending.substr(safe);
            }
        }

        llama_batch single = llama_batch_get_one(&tok, 1);
        pos++;
        int dc = llama_decode(m_ctx, single);
        if (dc != 0) break;
    }

    if (!pending.empty()) {
        size_t eot_pos = pending.find(EOT);
        if (eot_pos == std::string::npos) eot_pos = pending.find(EOS);
        if (eot_pos == std::string::npos) eot_pos = pending.size();
        if (eot_pos > 0) {
            cb(pending.substr(0, eot_pos));
            response += pending.substr(0, eot_pos);
        }
        pending.clear();
    }

    auto t_end = std::chrono::steady_clock::now();
    float elapsed_s = std::chrono::duration<float>(t_end - t_start).count();
    m_tps = (elapsed_s > 0.0f && n_generated > 0)
                ? static_cast<float>(n_generated) / elapsed_s
                : 0.0f;

    m_history.push_back({"user",      user_input});
    m_history.push_back({"assistant", response});
}

void Engine::clear_history() {
    m_history.clear();
}

size_t Engine::ram_usage_mb() const {
    return process_ram_mb();
}

std::string Engine::current_model_name() const {
    size_t pos = m_model_path.find_last_of("/\\");
    return (pos == std::string::npos) ? m_model_path : m_model_path.substr(pos + 1);
}
