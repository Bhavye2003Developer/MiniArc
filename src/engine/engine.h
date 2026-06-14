#pragma once
#include "scheduler/scheduler.h"
#include <functional>
#include <string>
#include <vector>
#include <cstddef>

struct ChatTurn {
    std::string role;    // "user" or "assistant"
    std::string content;
};

class Engine {
public:
    using TokenCB = std::function<void(const std::string& token_text)>;

    // Loads model from path. Calls std::exit(1) on failure.
    explicit Engine(const std::string& model_path, int n_ctx = 2048);
    ~Engine();

    // Streams response tokens via cb. Reads InferenceParams from scheduler
    // before each generated token — pauses automatically if paused==true.
    void generate(const std::string& user_input,
                  Scheduler&         scheduler,
                  const TokenCB&     cb);

    void clear_history();

    // Swap to a different GGUF file; preserves conversation history.
    // Returns false and prints error if new path fails to load.
    bool swap_model(const std::string& new_path);

    float  last_tokens_per_sec()  const { return m_tps; }
    size_t ram_usage_mb()         const;
    std::string current_model_name() const;

    // Not copyable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    void load(const std::string& path);
    void unload();

    std::string format_prompt(const std::string& user_input);

    struct llama_model*   m_model   = nullptr;
    struct llama_context* m_ctx     = nullptr;
    struct llama_sampler* m_sampler = nullptr;

    int m_n_ctx;
    std::vector<ChatTurn> m_history;
    float m_tps = 0.0f;
    std::string m_model_path;

    static const std::string SYSTEM_PROMPT;
};
