#include "ai_assist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "task.h"
#include "storage.h"
#include <time.h>
#include "llm_api.h"
#include "utils.h"

void ai_smart_add(const char *prompt, int debug) {
    // Get current date in ISO 8601 (UTC, no time)
    time_t now = time(NULL);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    char today_str[20];
    strftime(today_str, sizeof(today_str), "%Y-%m-%d", &tm_now);
    char iso_now[25];
    strftime(iso_now, sizeof(iso_now), "%Y-%m-%dT%H:%M:%SZ", &tm_now);
    if (debug) fprintf(stderr, "[ai_smart_add] Today (UTC): %s\n", today_str);

    // Compose system prompt with context for today
    char sys[2048];
    snprintf(sys, sizeof(sys),
        "Persona\n\n"
        "You are the AI engine powering a smart Todo CLI application.\n\n"
        "Instructions\n"
        "- Input: A user’s natural-language todo description.\n"
        "- Output: A single JSON object only, with no extra text or explanation.\n"
        "- Schema:\n"
        "  - id (string): a newly generated GUID (e.g. \"a18fb3d8-68f9-4760-97e4-bc932e8d8821\")\n"
        "  - name (string): the task description\n"
        "  - created (string): current UTC timestamp in ISO 8601 (e.g. \"2025-04-16T20:40:00Z\")\n"
        "  - due (string): ISO 8601 UTC timestamp if a due date exists, else an empty string\n"
        "  - tags (array of strings): any labels mentioned\n"
        "  - priority (string): one of \"low\", \"medium\", or \"high\"\n"
        "  - project (string): project name, default \"default\" if omitted\n"
        "  - status (string): either \"pending\" or \"done\"\n"
        "- Parsing rules:\n"
        "  - Interpret \"today\", \"tomorrow\", or \"next week\" as due dates, using the provided current date.\n"
        "  - Treat \"urgent,\" \"now,\" or \"immediately\" as priority: \"high\".\n"
        "  - If no due date is specified, set due to \"\".\n"
        "  - Always set created to the moment the JSON is generated.\n\n"
        "Context\n\n"
        "Today's date (UTC): %s\n\n"
        "Example\n"
        "- Input:\n"
        "Play with newest OpenAI models for coding, tag as AI Learning, due next week, urgent\n\n"
        "- Output:\n"
        "{\n"
        "  \"id\": \"a18fb3d8-68f9-4760-97e4-bc932e8d8821\",\n"
        "  \"name\": \"Play with newest OpenAI models for coding\",\n"
        "  \"created\": \"2025-04-16T20:40:00Z\",\n"
        "  \"due\": \"2025-04-23T00:00:00Z\",\n"
        "  \"tags\": [\"AI\", \"Learning\"],\n"
        "  \"priority\": \"high\",\n"
        "  \"project\": \"default\",\n"
        "  \"status\": \"pending\"\n"
        "}\n",
        today_str);
    // Call LLM and get structured response
    LlmChatResponse *llm_resp = NULL;
    if (llm_chat(sys, prompt, &llm_resp, debug, NULL) != 0 || !llm_resp || llm_resp->n_choices < 1) {
        if (debug) fprintf(stderr, "[ai_smart_add] LLM call failed\n");
        llm_chat_response_free(llm_resp);
        exit(1);
    }
    if (debug) fprintf(stderr, "[ai_smart_add] LLM content: %s\n", llm_resp->choices[0].message.content);
    // Parse the JSON object from LLM content
    Task *t = task_from_json(llm_resp->choices[0].message.content);
    llm_chat_response_free(llm_resp);
    if (!t) {
        if (debug) fprintf(stderr, "Could not parse task JSON from LLM. Content: %s\n", llm_resp->choices[0].message.content);
        exit(1);
    }
    if (t && (!t->project || t->project[0]=='\0')) {
        if (t->project) free(t->project);
        t->project = utils_strdup("default");
    }
    // Load, append, save
    size_t count = 0;
    Task **tasks = storage_load_tasks(&count);
    tasks = realloc(tasks, (count+2)*sizeof(Task*));
    tasks[count++] = t;
    tasks[count] = NULL;
    if (storage_save_tasks(tasks, count) != 0) {
        if (debug) fprintf(stderr, "Failed to save new task\n");
        storage_free_tasks(tasks, count);
        exit(1);
    }
    printf("AI task added: %s\n", t->name);
    storage_free_tasks(tasks, count);
}
