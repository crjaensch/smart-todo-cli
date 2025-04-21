#ifndef LLM_API_H
#define LLM_API_H

// for size_t
#include <stddef.h>

// Sends a chat completion request to the LLM. The response is allocated and must be freed by the caller.
// Returns 0 on success, nonzero on error.
// If debug is nonzero, logs HTTP and LLM details to stderr.
// If model is NULL, uses the default model (gpt-4.1-mini).

// --- Structured LLM response types ---

typedef struct {
    char *role;
    char *content;
} LlmMessage;

typedef struct {
    int        index;
    LlmMessage message;
    char      *finish_reason;
} LlmChoice;

typedef struct {
    long     http_status;
    char    *id;
    char    *object;
    long     created;
    char    *model;
    LlmChoice *choices;
    size_t    n_choices;
    struct {
        int prompt_tokens;
        int completion_tokens;
        int total_tokens;
    } usage;
    char    *raw_json;
} LlmChatResponse;

// Free a structured LLM response
void llm_chat_response_free(LlmChatResponse *resp);

// Chat with the LLM and get a structured response
int llm_chat(const char *system_prompt, const char *user_prompt, LlmChatResponse **response_obj, int debug, const char *model);

#endif // LLM_API_H
