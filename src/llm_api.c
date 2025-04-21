#include "llm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

/**
 * Structure to hold the memory chunk for curl write callback.
 */
struct curl_mem {
    char *ptr;
    size_t len;
};

/**
 * Write callback function for curl.
 * 
 * @param contents The contents to be written.
 * @param size The size of each element.
 * @param nmemb The number of elements.
 * @param userp The user-provided pointer.
 * 
 * @return The number of bytes written.
 */
static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_mem *mem = (struct curl_mem *)userp;
    char *ptr = realloc(mem->ptr, mem->len + realsize + 1);
    if (!ptr) return 0;
    mem->ptr = ptr;
    memcpy(&(mem->ptr[mem->len]), contents, realsize);
    mem->len += realsize;
    mem->ptr[mem->len] = 0;
    return realsize;
}

/**
 * Send a chat completion request to the OpenAI API.
 * 
 * @param system_prompt The system prompt.
 * @param user_prompt The user prompt.
 * @param response_obj The response from the API.
 * @param debug Whether to print debug messages.
 * @param model The model to use (optional, defaults to gpt-4.1-mini).
 * 
 * @return 0 on success, non-zero on failure.
 */
int llm_chat(const char *system_prompt, const char *user_prompt, LlmChatResponse **response_obj, int debug, const char *model) {
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        if (debug) fprintf(stderr, "OPENAI_API_KEY not set\n");
        return 1;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return 2;
    struct curl_mem chunk = {malloc(1), 0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    // Compose JSON body with system and user prompt using cJSON
    cJSON *root = cJSON_CreateObject();
    
    // Use the provided model or default to gpt-4.1-mini
    const char *model_to_use = model ? model : "gpt-4.1-mini";
    cJSON_AddStringToObject(root, "model", model_to_use);
    
    cJSON *msgs = cJSON_CreateArray();
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", system_prompt);
    cJSON_AddItemToArray(msgs, sys_msg);
    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", user_prompt);
    cJSON_AddItemToArray(msgs, user_msg);
    cJSON_AddItemToObject(root, "messages", msgs);
    cJSON_AddNumberToObject(root, "temperature", 0.0);
    char *json_body = cJSON_PrintUnformatted(root);
    if (debug) fprintf(stderr, "[llm_chat] Request JSON: %s\n", json_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (debug) fprintf(stderr, "[llm_chat] HTTP status: %ld\n", http_code);
    if (res != CURLE_OK) {
        if (debug) fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(chunk.ptr);
        cJSON_Delete(root);
        free(json_body);
        return 3;
    }
    if (debug) fprintf(stderr, "[llm_chat] Raw response: %s\n", chunk.ptr);
    // clean up request object
    cJSON_Delete(root);
    // parse JSON and build structured response
    cJSON *json = cJSON_Parse(chunk.ptr);
    if (!json) {
        free(chunk.ptr);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(json_body);
        return 4;
    }
    LlmChatResponse *resp = calloc(1, sizeof(*resp));
    resp->http_status = http_code;
    // top-level fields
    cJSON *item;
    item = cJSON_GetObjectItem(json, "id"); if (cJSON_IsString(item)) resp->id = strdup(item->valuestring);
    item = cJSON_GetObjectItem(json, "object"); if (cJSON_IsString(item)) resp->object = strdup(item->valuestring);
    item = cJSON_GetObjectItem(json, "created"); if (cJSON_IsNumber(item)) resp->created = item->valueint;
    item = cJSON_GetObjectItem(json, "model"); if (cJSON_IsString(item)) resp->model = strdup(item->valuestring);
    // choices array
    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (cJSON_IsArray(choices)) {
        resp->n_choices = cJSON_GetArraySize(choices);
        resp->choices = calloc(resp->n_choices, sizeof(*resp->choices));
        for (size_t i = 0; i < resp->n_choices; ++i) {
            cJSON *ch = cJSON_GetArrayItem(choices, i);
            cJSON *idx = cJSON_GetObjectItem(ch, "index"); if (cJSON_IsNumber(idx)) resp->choices[i].index = idx->valueint;
            cJSON *fr = cJSON_GetObjectItem(ch, "finish_reason"); if (cJSON_IsString(fr)) resp->choices[i].finish_reason = strdup(fr->valuestring);
            cJSON *msg = cJSON_GetObjectItem(ch, "message");
            if (cJSON_IsObject(msg)) {
                cJSON *role = cJSON_GetObjectItem(msg, "role"); if (cJSON_IsString(role)) resp->choices[i].message.role = strdup(role->valuestring);
                cJSON *content = cJSON_GetObjectItem(msg, "content"); if (cJSON_IsString(content)) resp->choices[i].message.content = strdup(content->valuestring);
            }
        }
    }
    // usage
    cJSON *usage = cJSON_GetObjectItem(json, "usage");
    if (cJSON_IsObject(usage)) {
        item = cJSON_GetObjectItem(usage, "prompt_tokens"); if (cJSON_IsNumber(item)) resp->usage.prompt_tokens = item->valueint;
        item = cJSON_GetObjectItem(usage, "completion_tokens"); if (cJSON_IsNumber(item)) resp->usage.completion_tokens = item->valueint;
        item = cJSON_GetObjectItem(usage, "total_tokens"); if (cJSON_IsNumber(item)) resp->usage.total_tokens = item->valueint;
    }
    resp->raw_json = chunk.ptr;
    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(json_body);
    *response_obj = resp;
    return 0;
}

// Free a structured LLM response
void llm_chat_response_free(LlmChatResponse *resp) {
    if (!resp) return;
    for (size_t i = 0; i < resp->n_choices; ++i) {
        free(resp->choices[i].message.role);
        free(resp->choices[i].message.content);
        free(resp->choices[i].finish_reason);
    }
    free(resp->choices);
    free(resp->id);
    free(resp->object);
    free(resp->model);
    free(resp->raw_json);
    free(resp);
}
