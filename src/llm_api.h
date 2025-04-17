#ifndef LLM_API_H
#define LLM_API_H

// Sends a chat completion request to the LLM. The response is allocated and must be freed by the caller.
// Returns 0 on success, nonzero on error.
// If debug is nonzero, logs HTTP and LLM details to stderr.
int llm_chat(const char *system_prompt, const char *user_prompt, char **response, int debug);

#endif // LLM_API_H
