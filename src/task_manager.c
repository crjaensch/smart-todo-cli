/**
 * @file task_manager.c
 * @brief Centralized task management functions implementation
 */

#include "task_manager.h"
#include "storage.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROJECTS 64
static char *project_list[MAX_PROJECTS];
static size_t project_count = 0;

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
                          Priority priority, const char *project) {
    if (!tasks || !*tasks || !count || !name) {
        return -1;
    }
    
    // Create the new task
    Task *new_task = task_create(name, due, tags, tag_count, priority);
    if (!new_task) {
        return -1;
    }
    
    if (new_task && project) {
        free(new_task->project);
        new_task->project = utils_strdup(project);
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

size_t task_manager_filter_by_project(Task **tasks, size_t count,
                                      const char *project,
                                      Task **filtered_tasks) {
    if (!tasks || !filtered_tasks || !project) return 0;
    size_t n = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(tasks[i]->project, project) == 0) {
            filtered_tasks[n++] = tasks[i];
        }
    }
    return n;
}

size_t task_manager_filter_by_date_range(Task **tasks, size_t count, 
                                        time_t start_date, time_t end_date,
                                        Task **filtered_tasks) {
    if (!tasks || !filtered_tasks) {
        return 0;
    }
    
    size_t filtered_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (!tasks[i] || tasks[i]->due == 0) {
            continue; // Skip tasks with no due date
        }
        
        // Check if task is within date range
        bool in_range = true;
        
        if (start_date > 0 && tasks[i]->due < start_date) {
            in_range = false; // Due date is before start date
        }
        
        if (end_date > 0 && tasks[i]->due > end_date) {
            in_range = false; // Due date is after end date
        }
        
        if (in_range) {
            filtered_tasks[filtered_count++] = tasks[i];
        }
    }
    
    return filtered_count;
}

// Helper function to get the start of a day (midnight)
static time_t get_start_of_day(time_t t) {
    struct tm tm_date;
    localtime_r(&t, &tm_date);
    tm_date.tm_hour = 0;
    tm_date.tm_min = 0;
    tm_date.tm_sec = 0;
    return mktime(&tm_date);
}

// Helper function to get the end of a day (23:59:59)
static time_t get_end_of_day(time_t t) {
    struct tm tm_date;
    localtime_r(&t, &tm_date);
    tm_date.tm_hour = 23;
    tm_date.tm_min = 59;
    tm_date.tm_sec = 59;
    return mktime(&tm_date);
}

size_t task_manager_filter_by_date_preset(Task **tasks, size_t count, 
                                         const char *range_type,
                                         Task **filtered_tasks) {
    if (!tasks || !filtered_tasks || !range_type) {
        return 0;
    }
    
    time_t now = time(NULL);
    time_t start_date = 0;
    time_t end_date = 0;
    
    // Calculate start and end dates based on range type
    if (strcasecmp(range_type, "today") == 0) {
        // Today: from midnight to 23:59:59 today
        start_date = get_start_of_day(now);
        end_date = get_end_of_day(now);
    } 
    else if (strcasecmp(range_type, "tomorrow") == 0) {
        // Tomorrow: from midnight to 23:59:59 tomorrow
        time_t tomorrow = now + 24 * 60 * 60; // Add one day in seconds
        start_date = get_start_of_day(tomorrow);
        end_date = get_end_of_day(tomorrow);
    } 
    else if (strcasecmp(range_type, "this_week") == 0) {
        // This week: from today to end of the week (Sunday)
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        // Start from today
        start_date = get_start_of_day(now);
        
        // Calculate days until end of week (Sunday)
        int days_to_end = 7 - tm_now.tm_wday;
        if (days_to_end == 0) days_to_end = 7; // If today is Sunday, go to next Sunday
        
        // End date is end of Sunday
        time_t end_of_week = now + days_to_end * 24 * 60 * 60;
        end_date = get_end_of_day(end_of_week);
    } 
    else if (strcasecmp(range_type, "next_week") == 0) {
        // Next week: from next Monday to next Sunday
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        // Calculate days until next Monday
        int days_to_monday = (7 - tm_now.tm_wday + 1) % 7;
        if (days_to_monday == 0) days_to_monday = 7; // If today is Monday, go to next Monday
        
        // Start date is beginning of next Monday
        time_t next_monday = now + days_to_monday * 24 * 60 * 60;
        start_date = get_start_of_day(next_monday);
        
        // End date is end of next Sunday (7 days after next Monday)
        time_t next_sunday = next_monday + 6 * 24 * 60 * 60;
        end_date = get_end_of_day(next_sunday);
    } 
    else if (strcasecmp(range_type, "overdue") == 0) {
        // Overdue: before today
        end_date = get_start_of_day(now) - 1; // End at 23:59:59 yesterday
    }
    else {
        // Unknown range type
        return 0;
    }
    
    // Use the date range filter with our calculated dates
    return task_manager_filter_by_date_range(tasks, count, start_date, end_date, filtered_tasks);
}

int task_manager_add_project(const char *name) {
    if (!name || strlen(name) == 0) return -1;
    for (size_t i = 0; i < project_count; ++i) {
        if (strcmp(project_list[i], name) == 0) return 0; // already exists
    }
    if (project_count >= MAX_PROJECTS) return -1;
    project_list[project_count++] = utils_strdup(name);
    return 0;
}

int task_manager_delete_project(const char *name, Task **tasks, size_t task_count) {
    if (!name || strlen(name) == 0) return -1;
    // Check if any task uses this project
    for (size_t i = 0; i < task_count; ++i) {
        if (tasks[i] && tasks[i]->project && strcmp(tasks[i]->project, name) == 0) {
            return -1; // In use
        }
    }
    // Find and remove from project_list
    for (size_t i = 0; i < project_count; ++i) {
        if (strcmp(project_list[i], name) == 0) {
            free(project_list[i]);
            for (size_t j = i + 1; j < project_count; ++j) {
                project_list[j-1] = project_list[j];
            }
            project_count--;
            return 0;
        }
    }
    return -1; // Not found
}

size_t task_manager_get_projects(char ***projects_out) {
    if (!projects_out) return 0;
    *projects_out = utils_malloc(project_count * sizeof(char*));
    for (size_t i = 0; i < project_count; ++i) {
        (*projects_out)[i] = project_list[i];
    }
    return project_count;
}

int task_manager_save_projects(void) {
    return storage_save_projects(project_list, project_count);
}

int task_manager_load_projects(void) {
    for (size_t i = 0; i < project_count; ++i) free(project_list[i]);
    project_count = 0;
    char **loaded = NULL;
    size_t n = storage_load_projects(&loaded);
    for (size_t i = 0; i < n && i < MAX_PROJECTS; ++i) {
        project_list[project_count++] = loaded[i];
    }
    free(loaded);
    return 0;
}

void task_manager_cleanup(Task **tasks, size_t count) {
    if (tasks) {
        storage_free_tasks(tasks, count);
    }
}
