/**
 * @file task_manager.c
 * @brief Centralized task management functions implementation
 */

#include "task_manager.h"
#include "storage.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

int task_manager_init(void) {
    return storage_init();
}

Task **task_manager_load_tasks(size_t *count) {
    return storage_load_tasks(count);
}

int task_manager_save_tasks(Task **tasks, size_t count) {
    return storage_save_tasks(tasks, count);
}

int task_manager_add_task(Task ***tasks, size_t *count, const char *name, 
                          time_t due, const char **tags, size_t tag_count, 
                          Priority priority) {
    if (!tasks || !*tasks || !count || !name) {
        return -1;
    }
    
    // Create the new task
    Task *new_task = task_create(name, due, tags, tag_count, priority);
    if (!new_task) {
        return -1;
    }
    
    // Allocate new array with space for the new task and NULL terminator
    Task **new_tasks = utils_malloc((*count + 2) * sizeof(Task*));
    if (!new_tasks) {
        task_free(new_task);
        return -1;
    }
    
    // Copy existing tasks to new array
    for (size_t i = 0; i < *count; i++) {
        new_tasks[i] = (*tasks)[i];
    }
    
    // Add new task and NULL terminator
    new_tasks[*count] = new_task;
    new_tasks[*count + 1] = NULL;
    
    // Free old array and update pointers
    free(*tasks);
    *tasks = new_tasks;
    (*count)++;
    
    return 0;
}

int task_manager_delete_task(Task ***tasks, size_t *count, size_t task_index) {
    if (!tasks || !*tasks || !count || task_index >= *count) {
        return -1;
    }
    
    // Free the task being deleted
    task_free((*tasks)[task_index]);
    
    // Shift remaining tasks
    for (size_t i = task_index; i < *count - 1; i++) {
        (*tasks)[i] = (*tasks)[i + 1];
    }
    
    // Decrement count
    (*count)--;
    
    // Reallocate the array to the new size (plus NULL terminator)
    Task **new_tasks = utils_realloc(*tasks, (*count + 1) * sizeof(Task*));
    if (new_tasks) {
        *tasks = new_tasks;
        (*tasks)[*count] = NULL; // Ensure NULL termination
    }
    
    return 0;
}

int task_manager_update_task(Task *task, const char *name, time_t due,
                             const char **tags, size_t tag_count,
                             int priority, int status) {
    if (!task) {
        return -1;
    }
    
    // Update name if provided
    if (name) {
        char *new_name = utils_strdup(name);
        if (!new_name) {
            return -1;
        }
        free(task->name);
        task->name = new_name;
    }
    
    // Update due date if provided (negative value means "don't change")
    if (due >= 0) {
        task->due = due;
    }
    
    // Update tags if provided
    if (tags) {
        // Free existing tags
        for (size_t i = 0; i < task->tag_count; i++) {
            free(task->tags[i]);
        }
        free(task->tags);
        
        // Allocate and copy new tags
        if (tag_count > 0) {
            task->tags = utils_malloc(tag_count * sizeof(char*));
            if (!task->tags) {
                task->tag_count = 0;
                return -1;
            }
            
            for (size_t i = 0; i < tag_count; i++) {
                task->tags[i] = utils_strdup(tags[i]);
                if (!task->tags[i]) {
                    // Clean up on failure
                    for (size_t j = 0; j < i; j++) {
                        free(task->tags[j]);
                    }
                    free(task->tags);
                    task->tags = NULL;
                    task->tag_count = 0;
                    return -1;
                }
            }
            task->tag_count = tag_count;
        } else {
            task->tags = NULL;
            task->tag_count = 0;
        }
    }
    
    // Update priority if provided (negative value means "don't change")
    if (priority >= 0 && priority <= PRIORITY_HIGH) {
        task->priority = (Priority)priority;
    }
    
    // Update status if provided (negative value means "don't change")
    if (status >= 0 && status <= STATUS_DONE) {
        task->status = (Status)status;
    }
    
    return 0;
}

Status task_manager_toggle_status(Task *task) {
    if (!task) {
        return STATUS_PENDING; // Default return value on error
    }
    
    task->status = (task->status == STATUS_DONE) ? STATUS_PENDING : STATUS_DONE;
    return task->status;
}

void task_manager_sort_by_name(Task **tasks, size_t count) {
    if (tasks && count > 0) {
        qsort(tasks, count, sizeof(Task*), task_compare_by_name);
    }
}

void task_manager_sort_by_due(Task **tasks, size_t count) {
    if (tasks && count > 0) {
        qsort(tasks, count, sizeof(Task*), task_compare_by_due);
    }
}

size_t task_manager_filter_by_search(Task **tasks, size_t count, 
                                    const char *search_term,
                                    Task **filtered_tasks) {
    if (!tasks || !filtered_tasks || !search_term) {
        return 0;
    }
    
    size_t filtered_count = 0;
    
    // If search term is empty, include all tasks
    if (search_term[0] == '\0') {
        for (size_t i = 0; i < count; i++) {
            filtered_tasks[filtered_count++] = tasks[i];
        }
        return filtered_count;
    }
    
    // Otherwise, filter by search term
    for (size_t i = 0; i < count; i++) {
        if (task_matches_search(tasks[i], search_term)) {
            filtered_tasks[filtered_count++] = tasks[i];
        }
    }
    
    return filtered_count;
}

void task_manager_cleanup(Task **tasks, size_t count) {
    if (tasks) {
        storage_free_tasks(tasks, count);
    }
}
