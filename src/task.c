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
    t->note = NULL; // Initialize note to NULL

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

    // Set default project
    t->project = utils_strdup("default");
    if (!t->project) {
        for (size_t j = 0; j < t->tag_count; ++j) free(t->tags[j]);
        free(t->tags);
        free(t->name);
        free(t->id);
        free(t);
        return NULL;
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
    free(t->project);
    free(t->note); // Free the note if it exists
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
    cJSON_AddStringToObject(obj, "project", t->project ? t->project : "default");
    
    // Add note if it exists, otherwise add null
    if (t->note) {
        cJSON_AddStringToObject(obj, "note", t->note);
    } else {
        cJSON_AddNullToObject(obj, "note");
    }

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
    cJSON *project = cJSON_GetObjectItem(obj, "project");
    cJSON *note = cJSON_GetObjectItem(obj, "note"); // Get note if it exists

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

    if (cJSON_IsString(project)) {
        t->project = utils_strdup(project->valuestring);
    } else {
        t->project = utils_strdup("default");
    }
    if (!t->project) {
        // cleanup memory
        free(t->name); free(t->id);
        for (size_t i = 0; i < t->tag_count; ++i) free(t->tags[i]);
        free(t->tags);
        free(t);
        cJSON_Delete(obj);
        return NULL;
    }

    // Handle note field
    if (note && cJSON_IsString(note)) {
        t->note = utils_strdup(note->valuestring);
        if (!t->note) {
            // cleanup memory
            free(t->project);
            free(t->name); free(t->id);
            for (size_t i = 0; i < t->tag_count; ++i) free(t->tags[i]);
            free(t->tags);
            free(t);
            cJSON_Delete(obj);
            return NULL;
        }
    } else {
        t->note = NULL; // No note or null note
    }

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
        return (t1->created > t2->created) - (t1->created < t2->created);
        
    // Tasks with no due date go last
    if (t1->due == 0) return 1;
    if (t2->due == 0) return -1;
    
    // Compare due dates
    return (t1->due > t2->due) - (t1->due < t2->due);
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

// Helper function to check if a task matches a specific filter
static bool task_matches_filter(const Task *t, const char *filter) {
    if (!t || !filter || filter[0] == '\0') return false;
    
    // Date filters
    if (strncmp(filter, "date:today", 10) == 0) {
        // Filter for tasks due today
        time_t now = time(NULL);
        struct tm tm_now, tm_due;
        localtime_r(&now, &tm_now);
        
        // Skip tasks with no due date
        if (t->due == 0) return false;
        
        localtime_r(&t->due, &tm_due);
        
        // Check if due date is today
        return (tm_now.tm_year == tm_due.tm_year && 
                tm_now.tm_mon == tm_due.tm_mon && 
                tm_now.tm_mday == tm_due.tm_mday);
    }
    else if (strncmp(filter, "date:tomorrow", 13) == 0) {
        // Filter for tasks due tomorrow
        time_t now = time(NULL);
        time_t tomorrow = now + 24 * 60 * 60; // Add one day in seconds
        struct tm tm_tomorrow, tm_due;
        localtime_r(&tomorrow, &tm_tomorrow);
        
        // Skip tasks with no due date
        if (t->due == 0) return false;
        
        localtime_r(&t->due, &tm_due);
        
        // Check if due date is tomorrow
        return (tm_tomorrow.tm_year == tm_due.tm_year && 
                tm_tomorrow.tm_mon == tm_due.tm_mon && 
                tm_tomorrow.tm_mday == tm_due.tm_mday);
    }
    else if (strncmp(filter, "date:this_week", 14) == 0) {
        // Filter for tasks due this week (today through Sunday)
        time_t now = time(NULL);
        struct tm tm_now, tm_due;
        localtime_r(&now, &tm_now);
        
        // Skip tasks with no due date
        if (t->due == 0) return false;
        
        localtime_r(&t->due, &tm_due);
        
        // Calculate start of today
        struct tm tm_start = tm_now;
        tm_start.tm_hour = 0;
        tm_start.tm_min = 0;
        tm_start.tm_sec = 0;
        time_t start_time = mktime(&tm_start);
        
        // Calculate end of week (end of Sunday)
        int days_to_end = 7 - tm_now.tm_wday;
        if (days_to_end == 0) days_to_end = 7; // If today is Sunday, go to next Sunday
        time_t end_of_week = start_time + days_to_end * 24 * 60 * 60 - 1; // -1 to get 23:59:59
        
        // Check if due date is within this week
        return (t->due >= start_time && t->due <= end_of_week);
    }
    else if (strncmp(filter, "date:next_week", 14) == 0) {
        // Filter for tasks due next week (next Monday through Sunday)
        time_t now = time(NULL);
        struct tm tm_now, tm_due;
        localtime_r(&now, &tm_now);
        
        // Skip tasks with no due date
        if (t->due == 0) return false;
        
        localtime_r(&t->due, &tm_due);
        
        // Calculate days until next Monday
        int days_to_monday = (7 - tm_now.tm_wday + 1) % 7;
        if (days_to_monday == 0) days_to_monday = 7; // If today is Monday, go to next Monday
        
        // Calculate start of next Monday
        struct tm tm_start = tm_now;
        tm_start.tm_hour = 0;
        tm_start.tm_min = 0;
        tm_start.tm_sec = 0;
        time_t next_monday = mktime(&tm_start) + days_to_monday * 24 * 60 * 60;
        
        // Calculate end of next Sunday
        time_t next_sunday = next_monday + 7 * 24 * 60 * 60 - 1; // -1 to get 23:59:59
        
        // Check if due date is within next week
        return (t->due >= next_monday && t->due <= next_sunday);
    }
    else if (strncmp(filter, "date:overdue", 12) == 0) {
        // Filter for overdue tasks (due before today)
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        // Skip tasks with no due date
        if (t->due == 0) return false;
        
        // Calculate start of today
        tm_now.tm_hour = 0;
        tm_now.tm_min = 0;
        tm_now.tm_sec = 0;
        time_t start_of_today = mktime(&tm_now);
        
        // Check if due date is before today
        return (t->due < start_of_today);
    }
    // Priority filters
    else if (strncmp(filter, "priority:high", 13) == 0) {
        return t->priority == PRIORITY_HIGH;
    }
    else if (strncmp(filter, "priority:medium", 15) == 0) {
        return t->priority == PRIORITY_MEDIUM;
    }
    else if (strncmp(filter, "priority:low", 12) == 0) {
        return t->priority == PRIORITY_LOW;
    }
    // Status filters
    else if (strncmp(filter, "status:done", 11) == 0) {
        return t->status == STATUS_DONE;
    }
    else if (strncmp(filter, "status:pending", 14) == 0) {
        return t->status == STATUS_PENDING;
    }
    
    // If it's not a recognized filter, check if it matches name or tags
    if (t->name && strcasestr(t->name, filter)) return true;
    
    // Check tags
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (strcasestr(t->tags[i], filter)) return true;
    }
    
    return false;
}

bool task_matches_search(const Task *t, const char *search_term) {
    if (!t || !search_term || search_term[0] == '\0') {
        return true; // Empty search matches everything
    }

    // Check if search term is a filter
    if (task_matches_filter(t, search_term)) {
        return true;
    }

    // Check name
    if (t->name && strcasestr(t->name, search_term)) {
        return true;
    }

    // Check tags
    for (size_t i = 0; i < t->tag_count; ++i) {
        if (t->tags[i] && strcasestr(t->tags[i], search_term)) {
            return true;
        }
    }

    // Check project
    if (t->project && strcasestr(t->project, search_term)) {
        return true;
    }

    // Check note
    if (t->note && strcasestr(t->note, search_term)) {
        return true;
    }

    return false;
}
    
/**
 * Set a note for a task.
 * @param task The task to set the note for
 * @param note The note text (will be copied)
 * @return 0 on success, -1 on error
 */
int task_set_note(Task *task, const char *note) {
    if (!task) return -1;
    
    // Free existing note if any
    if (task->note) {
        free(task->note);
        task->note = NULL;
    }
    
    // If note is NULL or empty, just leave the note as NULL
    if (!note || note[0] == '\0') {
        return 0;
    }
    
    // Copy the note
    task->note = utils_strdup(note);
    if (!task->note) {
        return -1; // Memory allocation failed
    }
    
    return 0;
}

/**
 * Get the note for a task.
 * @param task The task to get the note from
 * @return The note text or NULL if no note exists
 */
const char *task_get_note(const Task *task) {
    if (!task) return NULL;
    return task->note;
}
