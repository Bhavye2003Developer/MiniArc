#pragma once
#include "scheduler/scheduler.h"
#include <functional>
#include <string>
#include <vector>
#include <cstddef>

struct ChatTurn {
    std::string role;
    std::string content;
};

struct ModelConfig {
    float temperature      = 0.7f;
    int   top_k            = 40;
    float top_p            = 0.95f;
    float repeat_penalty   = 1.15f;
    int   max_prompt_tokens = 1500;  // max input tokens before history trimming kicks in
    int   max_new_tokens   = 512;    // max output tokens per response (0 = fill context)
};

class Engine {
public:
    using TokenCB = std::function<void(const std::string& token_text)>;

    explicit Engine(const std::string& model_path, int n_ctx = 2048);
    ~Engine();

    void generate(const std::string& user_input,
                  Scheduler&         scheduler,
                  const TokenCB&     cb);

    void clear_history();

    bool swap_model(const std::string& new_path);

    float  last_tokens_per_sec()  const { return m_tps; }
    size_t ram_usage_mb()         const;
    std::string current_model_name() const;

    ModelConfig get_config() const { return m_config; }
    void        set_config(const ModelConfig& cfg);

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    void load(const std::string& path);
    void unload();
    void rebuild_sampler();

    std::string format_prompt(const std::string& user_input);

    struct llama_model*   m_model   = nullptr;
    struct llama_context* m_ctx     = nullptr;
    struct llama_sampler* m_sampler = nullptr;

    int m_n_ctx = 2048;
    ModelConfig m_config;
    std::vector<ChatTurn> m_history;
    float m_tps = 0.0f;
    std::string m_model_path;

    static const std::string SYSTEM_PROMPT;
};
