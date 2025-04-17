#include "llm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>


struct curl_mem {
    char *ptr;
    size_t len;
};

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

int llm_chat(const char *system_prompt, const char *user_prompt, char **response, int debug) {
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
    cJSON_AddStringToObject(root, "model", "gpt-4.1-mini");
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
    *response = chunk.ptr;
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    cJSON_Delete(root);
    free(json_body);
    return 0;
}
