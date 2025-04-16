#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include <cjson/cJSON.h>
#include <time.h>

// Helper: convert Priority to string
static const char *priority_to_str(Priority p) {
    switch (p) {
        case PRIORITY_LOW: return "low";
        case PRIORITY_MEDIUM: return "medium";
        case PRIORITY_HIGH: return "high";
    }
    return "low";
}

// Helper: parse string to Priority
static Priority str_to_priority(const char *s) {
    if (strcmp(s, "high") == 0) return PRIORITY_HIGH;
    if (strcmp(s, "medium") == 0) return PRIORITY_MEDIUM;
    return PRIORITY_LOW;
}

// Helper: convert Status to string
static const char *status_to_str(Status s) {
    return s == STATUS_DONE ? "done" : "pending";
}

// Helper: parse string to Status
static Status str_to_status(const char *s) {
    return strcmp(s, "done") == 0 ? STATUS_DONE : STATUS_PENDING;
}

// Helper: format time_t as ISO8601
static char *time_to_iso8601(time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    char *buf = malloc(21);
    if (!buf) return NULL;
    strftime(buf, 21, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Helper: parse ISO8601 to time_t
static time_t iso8601_to_time(const char *s) {
    struct tm tm = {0};
    strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm);
#ifdef _GNU_SOURCE
    return timegm(&tm);
#else
    // timegm may be available on macOS
    return timegm(&tm);
#endif
}

Task *task_create(const char *name, time_t due, const char *tags[], size_t tag_count, Priority priority) {
    Task *t = calloc(1, sizeof(Task));
    if (!t) return NULL;
    
    // Generate UUID
    uuid_t binuuid;
    uuid_generate(binuuid);
    char uuid_str[37];
    uuid_unparse_lower(binuuid, uuid_str);
    t->id = strdup(uuid_str);
    
    t->name = strdup(name);
    t->created = time(NULL);
    t->due = due;
    t->priority = priority;
    t->status = STATUS_PENDING;

    t->tag_count = tag_count;
    if (tag_count > 0) {
        t->tags = malloc(tag_count * sizeof(char*));
        for (size_t i = 0; i < tag_count; ++i) {
            t->tags[i] = strdup(tags[i]);
        }
    } else {
        t->tags = NULL;
    }

    return t;
}

void task_free(Task *t) {
    if (!t) return;
    free(t->id);
    free(t->name);
    for (size_t i = 0; i < t->tag_count; ++i) {
        free(t->tags[i]);
    }
    free(t->tags);
    free(t);
}

char *task_to_json(const Task *t) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;

    cJSON_AddStringToObject(obj, "id", t->id);
    cJSON_AddStringToObject(obj, "name", t->name);

    char *created_str = time_to_iso8601(t->created);
    cJSON_AddStringToObject(obj, "created", created_str);
    free(created_str);

    if (t->due > 0) {
        char *due_str = time_to_iso8601(t->due);
        cJSON_AddStringToObject(obj, "due", due_str);
        free(due_str);
    } else {
        cJSON_AddNullToObject(obj, "due");
    }

    cJSON *tag_arr = cJSON_AddArrayToObject(obj, "tags");
    for (size_t i = 0; i < t->tag_count; ++i) {
        cJSON_AddItemToArray(tag_arr, cJSON_CreateString(t->tags[i]));
    }

    cJSON_AddStringToObject(obj, "priority", priority_to_str(t->priority));
    cJSON_AddStringToObject(obj, "status", status_to_str(t->status));

    char *json_str = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json_str;
}

Task *task_from_json(const char *json_str) {
    cJSON *obj = cJSON_Parse(json_str);
    if (!obj) return NULL;

    cJSON *id = cJSON_GetObjectItem(obj, "id");
    cJSON *name = cJSON_GetObjectItem(obj, "name");
    cJSON *created = cJSON_GetObjectItem(obj, "created");
    cJSON *due = cJSON_GetObjectItem(obj, "due");
    cJSON *tags = cJSON_GetObjectItem(obj, "tags");
    cJSON *priority = cJSON_GetObjectItem(obj, "priority");
    cJSON *status = cJSON_GetObjectItem(obj, "status");

    if (!cJSON_IsString(id) || !cJSON_IsString(name) || !cJSON_IsString(created)
        || !cJSON_IsArray(tags) || !cJSON_IsString(priority) || !cJSON_IsString(status)) {
        cJSON_Delete(obj);
        return NULL;
    }

    Task *t = calloc(1, sizeof(Task));
    if (!t) {
        cJSON_Delete(obj);
        return NULL;
    }

    t->id = strdup(id->valuestring);
    t->name = strdup(name->valuestring);
    t->created = iso8601_to_time(created->valuestring);

    if (cJSON_IsString(due)) {
        t->due = iso8601_to_time(due->valuestring);
    } else {
        t->due = 0;
    }

    // Tags
    size_t count = cJSON_GetArraySize(tags);
    t->tag_count = count;
    if (count > 0) {
        t->tags = malloc(count * sizeof(char*));
        for (size_t i = 0; i < count; ++i) {
            cJSON *item = cJSON_GetArrayItem(tags, i);
            if (cJSON_IsString(item)) {
                t->tags[i] = strdup(item->valuestring);
            } else {
                t->tags[i] = strdup("");
            }
        }
    }

    t->priority = str_to_priority(priority->valuestring);
    t->status = str_to_status(status->valuestring);

    cJSON_Delete(obj);
    return t;
}

int task_compare_by_name(const void *a, const void *b) {
    const Task *t1 = *(const Task **)a;
    const Task *t2 = *(const Task **)b;
    return strcmp(t1->name, t2->name);
}

int task_compare_by_creation(const void *a, const void *b) {
    const Task *t1 = *(const Task **)a;
    const Task *t2 = *(const Task **)b;
    if (t1->created < t2->created) return -1;
    if (t1->created > t2->created) return 1;
    return 0;
}

// Compare tasks by due date (ascending, 0 due dates go last)
int task_compare_by_due(const void *a, const void *b) {
    const Task *t1 = *(const Task **)a;
    const Task *t2 = *(const Task **)b;
    if (t1->due == 0 && t2->due == 0) return 0;
    if (t1->due == 0) return 1;
    if (t2->due == 0) return -1;
    if (t1->due < t2->due) return -1;
    if (t1->due > t2->due) return 1;
    // If due dates are equal, sort by priority (high first)
    if (t1->priority > t2->priority) return -1;
    if (t1->priority < t2->priority) return 1;
    return 0;
}

bool task_has_tag(const Task *t, const char *tag) {
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (strcmp(t->tags[i], tag) == 0) return true;
    }
    return false;
}

bool task_has_status(const Task *t, Status status) {
    return t->status == status;
}

bool task_matches_search(const Task *t, const char *search_term) {
    if (strcasestr(t->name, search_term)) return true;
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (strcasestr(t->tags[i], search_term)) return true;
    }
    return false;
}
