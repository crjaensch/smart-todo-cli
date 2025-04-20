#include "storage.h"
#include "task.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cjson/cJSON.h>

#define STORAGE_DIR ".todo-app"
#define TASKS_FILE  "tasks.json"
#define PROJECTS_FILE  "projects.json"

// Build full path for a given filename under $HOME/.todo-app
static char *build_path(const char *filename) {
    const char *home = getenv("HOME");
    if (!home) return NULL;
    size_t len = strlen(home) + 1 + strlen(STORAGE_DIR) + 1 + strlen(filename) + 1;
    char *path = utils_malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/%s/%s", home, STORAGE_DIR, filename);
    return path;
}

// Ensure the storage directory exists; return 0 on success
int storage_init(void) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    size_t len = strlen(home) + 1 + strlen(STORAGE_DIR) + 1;
    char *dir = utils_malloc(len);
    if (!dir) return -1;
    snprintf(dir, len, "%s/%s", home, STORAGE_DIR);
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) == -1) {
            free(dir);
            return -1;
        }
    }
    free(dir);
    return 0;
}

// Load tasks from tasks.json; returns NULL on error, count set to number
Task **storage_load_tasks(size_t *count) {
    *count = 0;
    char *path = build_path(TASKS_FILE);
    if (!path) return NULL;
    
    FILE *f = utils_fopen(path, "r");
    free(path);
    if (!f) {
        // No file yet: return empty list
        return utils_calloc(1, sizeof(Task *));
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    
    // Read file contents
    char *data = utils_malloc(size + 1);
    if (!data) { 
        utils_fclose(f); 
        return NULL; 
    }
    
    fread(data, 1, size, f);
    data[size] = '\0';
    utils_fclose(f);

    // Parse JSON
    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Create tasks array
    size_t n = cJSON_GetArraySize(root);
    Task **tasks = utils_malloc((n + 1) * sizeof(Task *));
    if (!tasks) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Parse each task
    for (size_t i = 0; i < n; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(item)) {
            tasks[i] = NULL;
            continue;
        }
        char *item_str = cJSON_PrintUnformatted(item);
        if (!item_str) {
            tasks[i] = NULL;
            continue;
        }
        tasks[i] = task_from_json(item_str);
        free(item_str);
    }
    
    // Null-terminate the array
    tasks[n] = NULL;
    *count = n;
    cJSON_Delete(root);
    return tasks;
}

// Save tasks to tasks.json; return 0 on success
int storage_save_tasks(Task **tasks, size_t count) {
    if (storage_init() != 0) return -1;
    
    char *path = build_path(TASKS_FILE);
    if (!path) return -1;

    cJSON *root = cJSON_CreateArray();
    if (!root) { 
        free(path); 
        return -1; 
    }
    
    // Add each task to the JSON array
    for (size_t i = 0; i < count; ++i) {
        if (!tasks[i]) continue;
        
        char *tstr = task_to_json(tasks[i]);
        if (!tstr) continue;
        
        cJSON *obj = cJSON_Parse(tstr);
        free(tstr);
        
        if (obj) {
            cJSON_AddItemToArray(root, obj);
        }
    }
    
    // Convert JSON to string
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!out) {
        free(path);
        return -1;
    }

    // Write to file
    FILE *f = utils_fopen(path, "w");
    if (!f) {
        free(out);
        free(path);
        return -1;
    }
    
    fwrite(out, 1, strlen(out), f);
    utils_fclose(f);
    free(out);
    free(path);
    return 0;
}

// Save array of project names to projects.json
int storage_save_projects(char **projects, size_t count) {
    char *path = build_path(PROJECTS_FILE);
    if (!path) return -1;
    cJSON *root = cJSON_CreateArray();
    for (size_t i = 0; i < count; ++i) {
        cJSON_AddItemToArray(root, cJSON_CreateString(projects[i]));
    }
    char *json = cJSON_PrintUnformatted(root);
    FILE *f = fopen(path, "w");
    free(path);
    if (!f) { cJSON_Delete(root); free(json); return -1; }
    fputs(json, f);
    fclose(f);
    cJSON_Delete(root);
    free(json);
    return 0;
}

// Load array of project names from projects.json
size_t storage_load_projects(char ***projects_out) {
    *projects_out = NULL;
    char *path = build_path(PROJECTS_FILE);
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = utils_malloc(len+1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return 0;
    size_t n = cJSON_GetArraySize(root);
    char **arr = utils_malloc(n * sizeof(char*));
    size_t actual = 0;
    for (size_t i = 0; i < n; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (cJSON_IsString(item)) {
            arr[actual++] = utils_strdup(item->valuestring);
        }
    }
    cJSON_Delete(root);
    *projects_out = arr;
    return actual;
}

// Free array of tasks
void storage_free_tasks(Task **tasks, size_t count) {
    if (!tasks) return;
    for (size_t i = 0; i < count; ++i) {
        task_free(tasks[i]);
    }
    free(tasks);
}
