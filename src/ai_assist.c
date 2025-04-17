#include "ai_assist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "task.h"
#include "storage.h"
#include <time.h>

#define AI_SMART_ADD_DEBUG 0

struct curl_mem {
    char *ptr;
    size_t len;
};

static size_t write_cb(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_mem *mem = (struct curl_mem *)userp;
    mem->ptr = realloc(mem->ptr, mem->len + realsize + 1);
    memcpy(&(mem->ptr[mem->len]), data, realsize);
    mem->len += realsize;
    mem->ptr[mem->len] = 0;
    return realsize;
}

void ai_smart_add(const char *prompt, int debug) {
    if (debug) fprintf(stderr, "[ai_smart_add] Called with prompt: %s\n", prompt);
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        if (debug) fprintf(stderr, "OPENAI_API_KEY not set\n");
        exit(1);
    }
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
        "  \"status\": \"pending\"\n"
        "}\n",
        today_str);
    char usermsg[512];
    snprintf(usermsg, sizeof(usermsg), "%s", prompt);
    // Build POST body
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "gpt-4.1-mini");
    cJSON *msgs = cJSON_CreateArray();
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", sys);
    cJSON_AddItemToArray(msgs, sys_msg);
    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", usermsg);
    cJSON_AddItemToArray(msgs, user_msg);
    cJSON_AddItemToObject(root, "messages", msgs);
    char *json_body = cJSON_PrintUnformatted(root);
    if (debug) fprintf(stderr, "[ai_smart_add] Request JSON: %s\n", json_body);

    CURL *curl = curl_easy_init();
    CURLcode res;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    struct curl_mem chunk = { malloc(1), 0 };
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (debug) fprintf(stderr, "[ai_smart_add] HTTP status: %ld\n", http_code);
    if (res != CURLE_OK) {
        if (debug) fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }
    if (debug) fprintf(stderr, "[ai_smart_add] Raw response: %s\n", chunk.ptr);
    // Parse response
    cJSON *resp = cJSON_Parse(chunk.ptr);
    if (!resp) {
        if (debug) fprintf(stderr, "OpenAI response not JSON. Raw: %s\n", chunk.ptr);
        exit(1);
    }
    cJSON *choices = cJSON_GetObjectItem(resp, "choices");
    if (!choices || !cJSON_IsArray(choices)) {
        if (debug) fprintf(stderr, "No choices. Full response: %s\n", chunk.ptr);
        exit(1);
    }
    cJSON *msg = cJSON_GetObjectItem(cJSON_GetArrayItem(choices, 0), "message");
    if (!msg) {
        if (debug) fprintf(stderr, "No message. Full response: %s\n", chunk.ptr);
        exit(1);
    }
    cJSON *content = cJSON_GetObjectItem(msg, "content");
    if (!content || !cJSON_IsString(content)) {
        if (debug) fprintf(stderr, "No content. Full response: %s\n", chunk.ptr);
        exit(1);
    }
    if (debug) fprintf(stderr, "[ai_smart_add] LLM content: %s\n", content->valuestring);
    // Parse the JSON object from content
    Task *t = task_from_json(content->valuestring);
    if (!t) {
        if (debug) fprintf(stderr, "Could not parse task JSON from LLM. Content: %s\n", content->valuestring);
        exit(1);
    }
    // Load, append, save
    size_t count = 0;
    Task **tasks = storage_load_tasks(&count);
    tasks = realloc(tasks, (count+2)*sizeof(Task*));
    tasks[count++] = t;
    tasks[count] = NULL;
    if (storage_save_tasks(tasks, count) != 0) {
        if (debug) fprintf(stderr, "Failed to save new task\n");
        exit(1);
    }
    printf("AI task added: %s\n", t->name);
    storage_free_tasks(tasks, count);
    free(chunk.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    cJSON_Delete(resp);
    free(json_body);
}
