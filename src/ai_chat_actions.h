#ifndef AI_CHAT_ACTIONS_H
#define AI_CHAT_ACTIONS_H

#include <cjson/cJSON.h>
#include "task.h"

// Common error message buffer size
#define MAX_ERR_LEN 256

// Action handler function results
typedef enum {
    ACTION_SUCCESS = 0,
    ACTION_ERROR = 1,
    ACTION_EXIT = 2  // Special case for exit action
} ActionResult;

// Add a new task based on JSON parameters
ActionResult handle_add_task(cJSON *params, Task ***tasks, size_t *count, 
                            const char *current_project, char *last_error);

// Delete a task by index
ActionResult handle_delete_task(cJSON *params, Task ***tasks, size_t *count, 
                               Task **disp, size_t disp_count, size_t *selected,
                               char *last_error);

// Edit an existing task
ActionResult handle_edit_task(cJSON *params, Task **disp, size_t disp_count, 
                             char *last_error);

// Mark a task as done
ActionResult handle_mark_done(cJSON *params, Task **disp, size_t disp_count, 
                             char *last_error);

// Edit a task's status
ActionResult handle_edit_task_status(cJSON *params, Task **disp, size_t disp_count, 
                                    char *last_error);

// Perform actions on the currently selected task
ActionResult handle_selected_task(cJSON *params, Task ***tasks, size_t *count, 
                                 Task **disp, size_t disp_count, size_t *selected, 
                                 size_t *new_selected, char *last_error);

// Sort tasks by a specified field
ActionResult handle_sort_tasks(cJSON *params, Task **tasks, size_t count, 
                              char *last_error);

// Filter tasks by due date
ActionResult handle_filter_by_date(cJSON *params, char *search_term, size_t term_size, 
                                  char *last_error);

// Filter tasks by priority
ActionResult handle_filter_by_priority(cJSON *params, char *search_term, size_t term_size, 
                                      char *last_error);

// Filter tasks by status
ActionResult handle_filter_by_status(cJSON *params, char *search_term, size_t term_size, 
                                    char *last_error);

// Apply multiple filters at once
ActionResult handle_filter_combined(cJSON *params, char *search_term, size_t term_size, 
                                   char *last_error);

// Search tasks by keyword
ActionResult handle_search_tasks(cJSON *params, char *search_term, size_t term_size, 
                                char *last_error);

// Clear search and show all tasks
ActionResult handle_list_tasks(char *search_term, char *last_error);

// Add a new project
ActionResult handle_add_project(cJSON *params, char ***projects, size_t *project_count, 
                               size_t *selected_project_idx, const char **current_project, 
                               Task **tasks, size_t count, char *last_error);

// Delete an existing project
ActionResult handle_delete_project(cJSON *params, char ***projects, size_t *project_count, 
                                  size_t *selected_project_idx, const char **current_project, 
                                  Task **tasks, size_t count, char *last_error);

// Add a note to a task
ActionResult handle_add_note(cJSON *params, Task **disp, size_t disp_count, char *last_error);

// View a note for a task
ActionResult handle_view_note(cJSON *params, Task **disp, size_t disp_count, char *last_error);

// Handle exit command
ActionResult handle_exit(cJSON *params, char *last_error);

#endif // AI_CHAT_ACTIONS_H
