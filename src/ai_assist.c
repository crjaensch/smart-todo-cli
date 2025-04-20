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
    char *llm_raw = NULL;
    int llm_status = llm_chat(sys, prompt, &llm_raw, debug, NULL);
    if (llm_status != 0 || !llm_raw) {
        if (debug) fprintf(stderr, "[ai_smart_add] LLM call failed\n");
        if (llm_raw) free(llm_raw);
        exit(1);
    }
    if (debug) fprintf(stderr, "[ai_smart_add] Raw LLM response: %s\n", llm_raw);
    // Parse response
    cJSON *resp = cJSON_Parse(llm_raw);
    if (!resp) {
        if (debug) fprintf(stderr, "OpenAI response not JSON. Raw: %s\n", llm_raw);
        free(llm_raw);
        exit(1);
    }
    cJSON *choices = cJSON_GetObjectItem(resp, "choices");
    if (!choices || !cJSON_IsArray(choices)) {
        if (debug) fprintf(stderr, "No choices. Full response: %s\n", llm_raw);
        cJSON_Delete(resp);
        free(llm_raw);
        exit(1);
    }
    cJSON *msg = cJSON_GetObjectItem(cJSON_GetArrayItem(choices, 0), "message");
    if (!msg) {
        if (debug) fprintf(stderr, "No message. Full response: %s\n", llm_raw);
        cJSON_Delete(resp);
        free(llm_raw);
        exit(1);
    }
    cJSON *content = cJSON_GetObjectItem(msg, "content");
    if (!content || !cJSON_IsString(content)) {
        if (debug) fprintf(stderr, "No content. Full response: %s\n", llm_raw);
        cJSON_Delete(resp);
        free(llm_raw);
        exit(1);
    }
    if (debug) fprintf(stderr, "[ai_smart_add] LLM content: %s\n", content->valuestring);
    // Parse the JSON object from content
    Task *t = task_from_json(content->valuestring);
    if (!t) {
        if (debug) fprintf(stderr, "Could not parse task JSON from LLM. Content: %s\n", content->valuestring);
        cJSON_Delete(resp);
        free(llm_raw);
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
        cJSON_Delete(resp);
        free(llm_raw);
        exit(1);
    }
    printf("AI task added: %s\n", t->name);
    storage_free_tasks(tasks, count);
    cJSON_Delete(resp);
    free(llm_raw);
}
