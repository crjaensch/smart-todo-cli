#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include <cjson/cJSON.h>
#include <time.h>
#include "utils.h"

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

Task *task_create(const char *name, time_t due, const char *tags[], size_t tag_count, Priority priority) {
    Task *t = utils_calloc(1, sizeof(Task));
    if (!t) return NULL;
    
    // Generate UUID
    uuid_t binuuid;
    uuid_generate(binuuid);
    char uuid_str[37];
    uuid_unparse_lower(binuuid, uuid_str);
    t->id = utils_strdup(uuid_str);
    if (!t->id) {
        free(t);
        return NULL;
    }
    
    t->name = utils_strdup(name);
    if (!t->name) {
        free(t->id);
        free(t);
        return NULL;
    }
    
    t->created = time(NULL);
    t->due = due;
    t->priority = priority;
    t->status = STATUS_PENDING;

    t->tag_count = tag_count;
    if (tag_count > 0) {
        t->tags = utils_malloc(tag_count * sizeof(char*));
        if (!t->tags) {
            free(t->name);
            free(t->id);
            free(t);
            return NULL;
        }
        
        for (size_t i = 0; i < tag_count; ++i) {
            t->tags[i] = utils_strdup(tags[i]);
            if (!t->tags[i]) {
                // Clean up already allocated tags
                for (size_t j = 0; j < i; ++j) {
                    free(t->tags[j]);
                }
                free(t->tags);
                free(t->name);
                free(t->id);
                free(t);
                return NULL;
            }
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

    char *created_str = utils_time_to_iso8601(t->created);
    if (!created_str) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(obj, "created", created_str);
    free(created_str);

    if (t->due > 0) {
        char *due_str = utils_time_to_iso8601(t->due);
        if (!due_str) {
            cJSON_Delete(obj);
            return NULL;
        }
        cJSON_AddStringToObject(obj, "due", due_str);
        free(due_str);
    } else {
        cJSON_AddNullToObject(obj, "due");
    }

    cJSON *tag_arr = cJSON_AddArrayToObject(obj, "tags");
    if (!tag_arr) {
        cJSON_Delete(obj);
        return NULL;
    }
    
    for (size_t i = 0; i < t->tag_count; ++i) {
        cJSON *tag_item = cJSON_CreateString(t->tags[i]);
        if (!tag_item) {
            cJSON_Delete(obj);
            return NULL;
        }
        cJSON_AddItemToArray(tag_arr, tag_item);
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

    Task *t = utils_calloc(1, sizeof(Task));
    if (!t) {
        cJSON_Delete(obj);
        return NULL;
    }

    t->id = utils_strdup(id->valuestring);
    if (!t->id) {
        free(t);
        cJSON_Delete(obj);
        return NULL;
    }
    
    t->name = utils_strdup(name->valuestring);
    if (!t->name) {
        free(t->id);
        free(t);
        cJSON_Delete(obj);
        return NULL;
    }
    
    t->created = utils_iso8601_to_time(created->valuestring);

    if (cJSON_IsString(due)) {
        t->due = utils_iso8601_to_time(due->valuestring);
    } else {
        t->due = 0;
    }

    // Tags
    size_t count = cJSON_GetArraySize(tags);
    t->tag_count = count;
    if (count > 0) {
        t->tags = utils_malloc(count * sizeof(char*));
        if (!t->tags) {
            free(t->name);
            free(t->id);
            free(t);
            cJSON_Delete(obj);
            return NULL;
        }
        
        for (size_t i = 0; i < count; ++i) {
            cJSON *item = cJSON_GetArrayItem(tags, i);
            if (cJSON_IsString(item)) {
                t->tags[i] = utils_strdup(item->valuestring);
                if (!t->tags[i]) {
                    // Clean up already allocated tags
                    for (size_t j = 0; j < i; ++j) {
                        free(t->tags[j]);
                    }
                    free(t->tags);
                    free(t->name);
                    free(t->id);
                    free(t);
                    cJSON_Delete(obj);
                    return NULL;
                }
            } else {
                t->tags[i] = utils_strdup("");
                if (!t->tags[i]) {
                    // Clean up already allocated tags
                    for (size_t j = 0; j < i; ++j) {
                        free(t->tags[j]);
                    }
                    free(t->tags);
                    free(t->name);
                    free(t->id);
                    free(t);
                    cJSON_Delete(obj);
                    return NULL;
                }
            }
        }
    } else {
        t->tags = NULL;
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
    
    // If both have no due date, compare by creation time
    if (t1->due == 0 && t2->due == 0)
        return task_compare_by_creation(a, b);
        
    // Tasks with no due date go last
    if (t1->due == 0) return 1;
    if (t2->due == 0) return -1;
    
    // Compare due dates
    if (t1->due < t2->due) return -1;
    if (t1->due > t2->due) return 1;
    
    // If due dates are equal, compare by creation time
    return task_compare_by_creation(a, b);
}

bool task_has_tag(const Task *t, const char *tag) {
    if (!t || !tag) return false;
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (strcasecmp(t->tags[i], tag) == 0) return true;
    }
    return false;
}

bool task_has_status(const Task *t, Status status) {
    return t && t->status == status;
}

bool task_matches_search(const Task *t, const char *search_term) {
    if (!t || !search_term || search_term[0] == '\0') return false;
    
    // Check name
    if (t->name && strcasestr(t->name, search_term)) return true;
    
    // Check tags
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (strcasestr(t->tags[i], search_term)) return true;
    }
    
    return false;
}
