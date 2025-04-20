#include "ai_chat_actions.h"
#include "task_manager.h"
#include "utils.h"
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <stdlib.h> // For free()

// Helper function to parse tags from JSON array
static int parse_tags_from_json(cJSON *tags, const char *tag_ptrs[], size_t max_tags) {
    int tag_count = 0;
    
    if (tags && cJSON_IsArray(tags)) {
        cJSON *tag_elem;
        cJSON_ArrayForEach(tag_elem, tags) {
            if (cJSON_IsString(tag_elem) && (size_t)tag_count < max_tags) {
                tag_ptrs[tag_count++] = tag_elem->valuestring;
            }
        }
    }
    
    return tag_count;
}

// Helper function to parse priority from JSON
static Priority parse_priority_from_json(cJSON *priority) {
    Priority prio = PRIORITY_LOW; // Default
    
    if (priority && cJSON_IsString(priority)) {
        if (strcasecmp(priority->valuestring, "high") == 0) {
            prio = PRIORITY_HIGH;
        } else if (strcasecmp(priority->valuestring, "medium") == 0) {
            prio = PRIORITY_MEDIUM;
        }
    }
    
    return prio;
}

// Helper function to parse due date from JSON
static time_t parse_due_date_from_json(cJSON *due) {
    time_t due_time = 0; // Default: no due date
    
    if (due) {
        if (cJSON_IsString(due)) {
            due_time = utils_parse_date(due->valuestring);
        } else if (cJSON_IsNull(due)) {
            due_time = 0; // Explicit null means clear the due date
        }
    }
    
    return due_time;
}

// Add a new task based on JSON parameters
ActionResult handle_add_task(cJSON *params, Task ***tasks, size_t *count, 
                            const char *current_project, char *last_error) {
    cJSON *name = cJSON_GetObjectItem(params, "name");
    cJSON *due = cJSON_GetObjectItem(params, "due");
    cJSON *tags = cJSON_GetObjectItem(params, "tags");
    cJSON *priority = cJSON_GetObjectItem(params, "priority");
    cJSON *project = cJSON_GetObjectItem(params, "project");

    // Validate required parameters
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
        snprintf(last_error, MAX_ERR_LEN, "Invalid params for add_task.");
        return ACTION_ERROR;
    }

    // Parse tags
    const char *tag_ptrs[16]; // Max 16 tags
    int tag_count = parse_tags_from_json(tags, tag_ptrs, 16);
    
    // Parse priority
    Priority prio = parse_priority_from_json(priority);
    
    // Parse due date
    time_t due_time = parse_due_date_from_json(due);
    
    // Parse project name
    const char *proj_name = current_project;
    if (project && cJSON_IsString(project) && project->valuestring[0] != '\0') {
        proj_name = project->valuestring;
    }

    // Add task using task manager
    if (task_manager_add_task(tasks, count, name->valuestring, due_time, tag_ptrs, tag_count, prio, proj_name) == 0) {
        utils_show_message("Task added.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Failed to add task.");
        return ACTION_ERROR;
    }
}

// Delete a task by index
ActionResult handle_delete_task(cJSON *params, Task ***tasks, size_t *count, 
                               Task **disp, size_t disp_count, size_t *selected,
                               char *last_error) {
    cJSON *index = cJSON_GetObjectItem(params, "index");
    
    if (!cJSON_IsNumber(index) || index->valueint <= 0 || index->valueint > (int)disp_count) {
        snprintf(last_error, MAX_ERR_LEN, "Invalid index for delete_task.");
        return ACTION_ERROR;
    }
    
    // Find the task in the original array
    Task *target_task = disp[index->valueint - 1]; // Convert to 0-based
    size_t task_index = 0;
    
    for (; task_index < *count; task_index++) {
        if ((*tasks)[task_index] == target_task) break;
    }
    
    if (task_index < *count) {
        if (task_manager_delete_task(tasks, count, task_index) == 0) {
            utils_show_message("Task deleted.", LINES - 2, 2);
            // Update selection if needed
            if (*selected >= disp_count - 1 && *selected > 0) {
                (*selected)--;
            }
            return ACTION_SUCCESS;
        } else {
            snprintf(last_error, MAX_ERR_LEN, "Failed to delete task.");
            return ACTION_ERROR;
        }
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Task not found in original array.");
        return ACTION_ERROR;
    }
}

// Edit an existing task
ActionResult handle_edit_task(cJSON *params, Task **disp, size_t disp_count, 
                             char *last_error) {
    cJSON *index = cJSON_GetObjectItem(params, "index");
    
    if (!cJSON_IsNumber(index) || index->valueint <= 0 || index->valueint > (int)disp_count) {
        snprintf(last_error, MAX_ERR_LEN, "Invalid index for edit_task.");
        return ACTION_ERROR;
    }
    
    Task *target_task = disp[index->valueint - 1]; // Convert to 0-based
    
    // Get optional edit fields
    cJSON *name = cJSON_GetObjectItem(params, "name");
    cJSON *due = cJSON_GetObjectItem(params, "due");
    cJSON *tags = cJSON_GetObjectItem(params, "tags");
    cJSON *priority = cJSON_GetObjectItem(params, "priority");
    cJSON *status = cJSON_GetObjectItem(params, "status");
    
    // Parse name
    const char *new_name = NULL;
    if (cJSON_IsString(name) && name->valuestring[0] != '\0') {
        new_name = name->valuestring;
    }
    
    // Parse due date
    time_t due_time = -1; // -1 means don't change
    if (due) {
        if (cJSON_IsString(due)) {
            due_time = utils_parse_date(due->valuestring);
        } else if (cJSON_IsNull(due)) {
            due_time = 0; // Explicit null means clear the due date
        }
    }
    
    // Parse tags
    const char *tag_ptrs[16]; // Max 16 tags
    size_t tag_count = 0;
    bool update_tags = false;
    
    if (tags && cJSON_IsArray(tags)) {
        update_tags = true;
        tag_count = parse_tags_from_json(tags, tag_ptrs, 16);
    }
    
    // Parse priority
    int prio = -1; // -1 means don't change
    if (priority && cJSON_IsString(priority)) {
        if (strcasecmp(priority->valuestring, "high") == 0) prio = PRIORITY_HIGH;
        else if (strcasecmp(priority->valuestring, "medium") == 0) prio = PRIORITY_MEDIUM;
        else if (strcasecmp(priority->valuestring, "low") == 0) prio = PRIORITY_LOW;
    }
    
    // Parse status
    int task_status = -1; // -1 means don't change
    if (status && cJSON_IsString(status)) {
        if (strcasecmp(status->valuestring, "done") == 0) task_status = STATUS_DONE;
        else if (strcasecmp(status->valuestring, "pending") == 0) task_status = STATUS_PENDING;
    }
    
    // Update the task
    if (task_manager_update_task(target_task, new_name, due_time, 
                               update_tags ? tag_ptrs : NULL, 
                               tag_count, prio, task_status) == 0) {
        utils_show_message("Task updated.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Failed to update task.");
        return ACTION_ERROR;
    }
}

// Mark a task as done
ActionResult handle_mark_done(cJSON *params, Task **disp, size_t disp_count, 
                             char *last_error) {
    cJSON *index_item = cJSON_GetObjectItem(params, "index");
    
    if (!cJSON_IsNumber(index_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing/invalid 'index' param for mark_done.");
        return ACTION_ERROR;
    }
    
    int index_1based = (int)index_item->valuedouble;
    if (index_1based < 1 || (size_t)index_1based > disp_count) {
        char index_error[MAX_ERR_LEN];
        snprintf(index_error, MAX_ERR_LEN, "Invalid index %d. Valid range: 1-%zu.", index_1based, disp_count);
        snprintf(last_error, MAX_ERR_LEN, "%s", index_error);
        return ACTION_ERROR;
    }
    
    size_t index_0based = index_1based - 1;
    Task *target_task = disp[index_0based]; // Get task from the *displayed* list
    
    // Use task manager to update status
    if (task_manager_update_task(target_task, NULL, -1, NULL, 0, -1, STATUS_DONE) == 0) {
        utils_show_message("Task marked done.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Failed to mark task as done.");
        return ACTION_ERROR;
    }
}

// Edit a task's status
ActionResult handle_edit_task_status(cJSON *params, Task **disp, size_t disp_count, 
                                    char *last_error) {
    cJSON *index_item = cJSON_GetObjectItem(params, "index");
    
    if (!cJSON_IsNumber(index_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing/invalid 'index' param for edit_task_status.");
        return ACTION_ERROR;
    }
    
    int index_1based = (int)index_item->valuedouble;
    if (index_1based <= 0 || index_1based > (int)disp_count) {
        char index_error[MAX_ERR_LEN];
        snprintf(index_error, MAX_ERR_LEN, "Invalid index %d. Valid range: 1-%zu.", index_1based, disp_count);
        snprintf(last_error, MAX_ERR_LEN, "%s", index_error);
        return ACTION_ERROR;
    }
    
    size_t index_0based = index_1based - 1;
    Task *target_task = disp[index_0based]; // Get task from the *displayed* list

    cJSON *status_item = cJSON_GetObjectItem(params, "status");
    if (!cJSON_IsString(status_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing 'status' param for edit_task_status.");
        return ACTION_ERROR;
    }
    
    int new_status = -1;
    if (strcasecmp(status_item->valuestring, "done") == 0) {
        new_status = STATUS_DONE;
    } else if (strcasecmp(status_item->valuestring, "pending") == 0) {
        new_status = STATUS_PENDING;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Invalid status value in edit_task_status.");
        return ACTION_ERROR;
    }
    
    // Use task manager to update status
    if (task_manager_update_task(target_task, NULL, -1, NULL, 0, -1, new_status) == 0) {
        utils_show_message("Task status updated.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Failed to update task status.");
        return ACTION_ERROR;
    }
}

// Perform actions on the currently selected task
ActionResult handle_selected_task(cJSON *params, Task ***tasks, size_t *count, 
                                 Task **disp, size_t disp_count, size_t *selected, 
                                 size_t *new_selected, char *last_error) {
    (void)new_selected;
    
    // Check if there are tasks available
    if (disp_count == 0) {
        snprintf(last_error, MAX_ERR_LEN, "No tasks available to select.");
        return ACTION_ERROR;
    }
    
    // Check if selection is valid
    if (*selected >= disp_count) {
        snprintf(last_error, MAX_ERR_LEN, "Invalid selection index.");
        return ACTION_ERROR;
    }
    
    // Get the nested action and params
    cJSON *nested_action_item = cJSON_GetObjectItem(params, "action");
    cJSON *nested_params_item = cJSON_GetObjectItem(params, "params");

    if (!cJSON_IsString(nested_action_item) || !cJSON_IsObject(nested_params_item)) {
        snprintf(last_error, MAX_ERR_LEN, "selected_task requires 'action' and 'params' fields.");
        return ACTION_ERROR;
    }
    
    const char *nested_action = nested_action_item->valuestring;
    cJSON *nested_params = nested_params_item;
    
    // Handle the nested action on the selected task
    Task *selected_task = disp[*selected];
   
    if (strcmp(nested_action, "mark_done") == 0) {
        // Update task status using task manager
        if (task_manager_update_task(selected_task, NULL, -1, NULL, 0, -1, STATUS_DONE) == 0) {
            utils_show_message("Selected task marked as done.", LINES - 2, 2);
            return ACTION_SUCCESS;
        } else {
            snprintf(last_error, MAX_ERR_LEN, "Failed to mark selected task as done.");
            return ACTION_ERROR;
        }
    } else if (strcmp(nested_action, "delete_task") == 0) {
        // Find the task in the original array
        size_t task_index = 0;
        for (; task_index < *count; task_index++) {
            if ((*tasks)[task_index] == selected_task) break;
        }
        
        if (task_index < *count) {
            if (task_manager_delete_task(tasks, count, task_index) == 0) {
                utils_show_message("Selected task deleted.", LINES - 2, 2);
                // Update selection if needed
                if (*selected >= disp_count - 1 && *selected > 0) {
                    (*selected)--;
                }
                return ACTION_SUCCESS;
            } else {
                snprintf(last_error, MAX_ERR_LEN, "Failed to delete selected task.");
                return ACTION_ERROR;
            }
        } else {
            snprintf(last_error, MAX_ERR_LEN, "Selected task not found in original array.");
            return ACTION_ERROR;
        }
    } else if (strcmp(nested_action, "edit_task") == 0) {
        // Get optional edit fields
        cJSON *name = cJSON_GetObjectItem(nested_params, "name");
        cJSON *due = cJSON_GetObjectItem(nested_params, "due");
        cJSON *tags = cJSON_GetObjectItem(nested_params, "tags");
        cJSON *priority = cJSON_GetObjectItem(nested_params, "priority");
        cJSON *status = cJSON_GetObjectItem(nested_params, "status");
        
        // Parse name
        const char *new_name = NULL;
        if (cJSON_IsString(name) && name->valuestring[0] != '\0') {
            new_name = name->valuestring;
        }
        
        // Parse due date
        time_t due_time = -1; // -1 means don't change
        if (due) {
            if (cJSON_IsString(due)) {
                due_time = utils_parse_date(due->valuestring);
            } else if (cJSON_IsNull(due)) {
                due_time = 0; // Explicit null means clear the due date
            }
        }
        
        // Parse tags
        const char *tag_ptrs[16]; // Max 16 tags
        size_t tag_count = 0;
        bool update_tags = false;
        
        if (tags && cJSON_IsArray(tags)) {
            update_tags = true;
            tag_count = parse_tags_from_json(tags, tag_ptrs, 16);
        }
        
        // Parse priority
        int prio = -1; // -1 means don't change
        if (priority && cJSON_IsString(priority)) {
            if (strcasecmp(priority->valuestring, "high") == 0) prio = PRIORITY_HIGH;
            else if (strcasecmp(priority->valuestring, "medium") == 0) prio = PRIORITY_MEDIUM;
            else if (strcasecmp(priority->valuestring, "low") == 0) prio = PRIORITY_LOW;
        }
        
        // Parse status
        int task_status = -1; // -1 means don't change
        if (status && cJSON_IsString(status)) {
            if (strcasecmp(status->valuestring, "done") == 0) task_status = STATUS_DONE;
            else if (strcasecmp(status->valuestring, "pending") == 0) task_status = STATUS_PENDING;
        }
        
        // Update the task
        if (task_manager_update_task(selected_task, new_name, due_time, 
                                   update_tags ? tag_ptrs : NULL, 
                                   tag_count, prio, task_status) == 0) {
            utils_show_message("Selected task updated.", LINES - 2, 2);
            return ACTION_SUCCESS;
        } else {
            snprintf(last_error, MAX_ERR_LEN, "Failed to update selected task.");
            return ACTION_ERROR;
        }
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Unsupported action for selected_task: %s", nested_action);
        return ACTION_ERROR;
    }
}

// Sort tasks by a specified field
ActionResult handle_sort_tasks(cJSON *params, Task **tasks, size_t count, 
                              char *last_error) {
    cJSON *by_item = cJSON_GetObjectItem(params, "by");
    if (!cJSON_IsString(by_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing 'by' parameter for sort_tasks.");
        return ACTION_ERROR;
    }
    
    if (strcasecmp(by_item->valuestring, "name") == 0) {
        task_manager_sort_by_name(tasks, count);
        utils_show_message("Tasks sorted by name.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else if (strcasecmp(by_item->valuestring, "due") == 0 || 
               strcasecmp(by_item->valuestring, "creation") == 0) {
        task_manager_sort_by_due(tasks, count);
        utils_show_message("Tasks sorted by due date.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Invalid sort field: %s. Use 'name', 'due', or 'creation'.", 
                by_item->valuestring);
        return ACTION_ERROR;
    }
}

// Filter tasks by due date
ActionResult handle_filter_by_date(cJSON *params, char *search_term, size_t term_size, 
                                  char *last_error) {
    cJSON *range_item = cJSON_GetObjectItem(params, "range");
    if (!cJSON_IsString(range_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing or invalid 'range' parameter for filter_by_date.");
        return ACTION_ERROR;
    }
    
    const char *range_type = range_item->valuestring;
    
    // Set the search term to indicate the date filter
    snprintf(search_term, term_size, "[date:%s]", range_type);
    
    // Display success message
    char msg[MAX_ERR_LEN];
    snprintf(msg, MAX_ERR_LEN, "Filtering tasks due %s.", 
             strcasecmp(range_type, "overdue") == 0 ? "overdue" : 
             (strcasecmp(range_type, "this_week") == 0 ? "this week" : 
              (strcasecmp(range_type, "next_week") == 0 ? "next week" : range_type)));
    utils_show_message(msg, LINES - 2, 2);
    
    return ACTION_SUCCESS;
}

// Filter tasks by priority
ActionResult handle_filter_by_priority(cJSON *params, char *search_term, size_t term_size, 
                                      char *last_error) {
    cJSON *level_item = cJSON_GetObjectItem(params, "level");
    if (!cJSON_IsString(level_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing or invalid 'level' parameter for filter_by_priority.");
        return ACTION_ERROR;
    }
    
    const char *level_type = level_item->valuestring;
    
    // Set the search term to indicate the priority filter
    snprintf(search_term, term_size, "[priority:%s]", level_type);
    
    // Display success message
    char msg[MAX_ERR_LEN];
    snprintf(msg, MAX_ERR_LEN, "Filtering tasks by priority: %s", level_type);
    utils_show_message(msg, LINES - 2, 2);
    
    return ACTION_SUCCESS;
}

// Filter tasks by status
ActionResult handle_filter_by_status(cJSON *params, char *search_term, size_t term_size, 
                                    char *last_error) {
    cJSON *status_item = cJSON_GetObjectItem(params, "status");
    if (!cJSON_IsString(status_item)) {
        snprintf(last_error, MAX_ERR_LEN, "Missing or invalid 'status' parameter for filter_by_status.");
        return ACTION_ERROR;
    }
    
    const char *status_type = status_item->valuestring;
    
    // Set the search term to indicate the status filter
    snprintf(search_term, term_size, "[status:%s]", status_type);
    
    // Display success message
    char msg[MAX_ERR_LEN];
    snprintf(msg, MAX_ERR_LEN, "Filtering tasks by status: %s", status_type);
    utils_show_message(msg, LINES - 2, 2);
    
    return ACTION_SUCCESS;
}

// Apply multiple filters at once
ActionResult handle_filter_combined(cJSON *params, char *search_term, size_t term_size, 
                                   char *last_error) {
    cJSON *filters_array = cJSON_GetObjectItem(params, "filters");
    if (!cJSON_IsArray(filters_array) || cJSON_GetArraySize(filters_array) <= 0) {
        snprintf(last_error, MAX_ERR_LEN, "Missing or invalid 'filters' array for filter_combined.");
        return ACTION_ERROR;
    }
    
    // Clear the search term first
    search_term[0] = '\0';
    
    // Build the combined filter
    int filter_count = 0;
    for (int i = 0; i < cJSON_GetArraySize(filters_array); i++) {
        cJSON *filter_obj = cJSON_GetArrayItem(filters_array, i);
        cJSON *type_item = cJSON_GetObjectItem(filter_obj, "type");
        cJSON *value_item = cJSON_GetObjectItem(filter_obj, "value");
        
        if (cJSON_IsString(type_item) && cJSON_IsString(value_item)) {
            const char *type = type_item->valuestring;
            const char *value = value_item->valuestring;
            
            // Add this filter to the search term
            char filter_part[64];
            
            if (strcmp(type, "date") == 0) {
                snprintf(filter_part, sizeof(filter_part), "[date:%s]", value);
            } else if (strcmp(type, "priority") == 0) {
                snprintf(filter_part, sizeof(filter_part), "[priority:%s]", value);
            } else if (strcmp(type, "status") == 0) {
                snprintf(filter_part, sizeof(filter_part), "[status:%s]", value);
            } else {
                continue; // Skip unknown filter type
            }
            
            // Append to search term if there's room
            if (strlen(search_term) + strlen(filter_part) < term_size - 1) {
                strcat(search_term, filter_part);
                filter_count++;
            }
        }
    }
    
    if (filter_count > 0) {
        // Display success message
        char msg[MAX_ERR_LEN];
        snprintf(msg, MAX_ERR_LEN, "Applied %d combined filters.", filter_count);
        utils_show_message(msg, LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "No valid filters found in the combined filter.");
        return ACTION_ERROR;
    }
}

// Search tasks by keyword
ActionResult handle_search_tasks(cJSON *params, char *search_term, size_t term_size, 
                                char *last_error) {
    cJSON *term_item = cJSON_GetObjectItem(params, "term");
    
    if (cJSON_IsString(term_item)) {
        strncpy(search_term, term_item->valuestring, term_size - 1);
        search_term[term_size - 1] = '\0'; // Ensure null termination
        utils_show_message("Search applied.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else if (cJSON_IsNull(term_item)) {
        search_term[0] = '\0'; // Clear search
        utils_show_message("Search cleared.", LINES - 2, 2);
        return ACTION_SUCCESS;
    } else {
        snprintf(last_error, MAX_ERR_LEN, "Invalid 'term' param for search_tasks.");
        return ACTION_ERROR;
    }
}

// Clear search and show all tasks
ActionResult handle_list_tasks(char *search_term, char *last_error) {
    (void)last_error;
    
    search_term[0] = '\0'; // Clear search term
    utils_show_message("Displaying all tasks.", LINES - 2, 2);
    return ACTION_SUCCESS;
}

// Add a new project
ActionResult handle_add_project(cJSON *params, char ***projects, size_t *project_count, 
                               size_t *selected_project_idx, const char **current_project, 
                               Task **tasks, size_t count, char *last_error) {
    (void)tasks;
    (void)count;
    (void)last_error;
    
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(params, "name");
    
    if (!cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        utils_show_message("Project name missing or invalid", LINES-2, 2);
        return ACTION_ERROR;
    }
    
    if (task_manager_add_project(name_item->valuestring) == 0) {
        task_manager_save_projects();
        free(*projects);
        *project_count = task_manager_get_projects(projects);
        *selected_project_idx = *project_count - 1;
        *current_project = (*projects)[*selected_project_idx];
        utils_show_message("Project created", LINES-2, 2);
        return ACTION_SUCCESS;
    } else {
        utils_show_message("Failed to create project", LINES-2, 2);
        return ACTION_ERROR;
    }
}

// Delete an existing project
ActionResult handle_delete_project(cJSON *params, char ***projects, size_t *project_count, 
                                  size_t *selected_project_idx, const char **current_project, 
                                  Task **tasks, size_t count, char *last_error) {
    (void)last_error;
    
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(params, "name");
    
    if (!cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        utils_show_message("Project name missing or invalid", LINES-2, 2);
        return ACTION_ERROR;
    }
    
    int del_result = task_manager_delete_project(name_item->valuestring, tasks, count);
    if (del_result == 0) {
        task_manager_save_projects();
        free(*projects);
        *project_count = task_manager_get_projects(projects);
        if (*selected_project_idx >= *project_count) *selected_project_idx = *project_count - 1;
        *current_project = (*projects)[*selected_project_idx];
        utils_show_message("Project deleted", LINES-2, 2);
        return ACTION_SUCCESS;
    } else {
        utils_show_message("Only projects without tasks can be deleted", LINES-2, 2);
        return ACTION_ERROR;
    }
}

// Handle exit command
ActionResult handle_exit(cJSON *params, char *last_error) {
    (void)params;
    (void)last_error;
    
    utils_show_message("Exiting AI chat mode...", LINES - 2, 1);
    return ACTION_EXIT;
}
