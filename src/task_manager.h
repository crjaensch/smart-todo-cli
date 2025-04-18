/**
 * @file task_manager.h
 * @brief Centralized task management functions
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "task.h"
#include <stdbool.h>

/**
 * Initialize the task manager
 * @return 0 on success, -1 on failure
 */
int task_manager_init(void);

/**
 * Load tasks from storage
 * @param count Pointer to store the number of tasks loaded
 * @return Array of tasks, NULL on failure
 */
Task **task_manager_load_tasks(size_t *count);

/**
 * Save tasks to storage
 * @param tasks Array of tasks to save
 * @param count Number of tasks in the array
 * @return 0 on success, -1 on failure
 */
int task_manager_save_tasks(Task **tasks, size_t count);

/**
 * Add a new task to the task array
 * @param tasks Pointer to task array (will be reallocated)
 * @param count Pointer to task count (will be incremented)
 * @param name Task name
 * @param due Due date (0 for none)
 * @param tags Array of tag strings
 * @param tag_count Number of tags
 * @param priority Task priority
 * @return 0 on success, -1 on failure
 */
int task_manager_add_task(Task ***tasks, size_t *count, const char *name, 
                          time_t due, const char **tags, size_t tag_count, 
                          Priority priority);

/**
 * Delete a task from the task array
 * @param tasks Pointer to task array (will be reallocated)
 * @param count Pointer to task count (will be decremented)
 * @param task_index Index of the task to delete
 * @return 0 on success, -1 on failure
 */
int task_manager_delete_task(Task ***tasks, size_t *count, size_t task_index);

/**
 * Update a task's properties
 * @param task Task to update
 * @param name New name (NULL to keep current)
 * @param due New due date (negative to keep current)
 * @param tags New tags (NULL to keep current)
 * @param tag_count Number of new tags
 * @param priority New priority (negative to keep current)
 * @param status New status (negative to keep current)
 * @return 0 on success, -1 on failure
 */
int task_manager_update_task(Task *task, const char *name, time_t due,
                             const char **tags, size_t tag_count,
                             int priority, int status);

/**
 * Toggle a task's status between done and pending
 * @param task Task to toggle
 * @return New status
 */
Status task_manager_toggle_status(Task *task);

/**
 * Sort tasks by name
 * @param tasks Task array to sort
 * @param count Number of tasks
 */
void task_manager_sort_by_name(Task **tasks, size_t count);

/**
 * Sort tasks by due date
 * @param tasks Task array to sort
 * @param count Number of tasks
 */
void task_manager_sort_by_due(Task **tasks, size_t count);

/**
 * Filter tasks by search term
 * @param tasks Source task array
 * @param count Number of tasks in source array
 * @param search_term Term to search for
 * @param filtered_tasks Output array for filtered tasks (must be pre-allocated)
 * @return Number of tasks in filtered array
 */
size_t task_manager_filter_by_search(Task **tasks, size_t count, 
                                    const char *search_term,
                                    Task **filtered_tasks);

/**
 * Filter tasks by date range
 * @param tasks Source task array
 * @param count Number of tasks in source array
 * @param start_date Start of date range (inclusive, 0 for no lower bound)
 * @param end_date End of date range (inclusive, 0 for no upper bound)
 * @param filtered_tasks Output array for filtered tasks (must be pre-allocated)
 * @return Number of tasks in filtered array
 */
size_t task_manager_filter_by_date_range(Task **tasks, size_t count, 
                                        time_t start_date, time_t end_date,
                                        Task **filtered_tasks);

/**
 * Filter tasks by predefined date range
 * @param tasks Source task array
 * @param count Number of tasks in source array
 * @param range_type Predefined range: "today", "tomorrow", "this_week", "next_week", "overdue"
 * @param filtered_tasks Output array for filtered tasks (must be pre-allocated)
 * @return Number of tasks in filtered array
 */
size_t task_manager_filter_by_date_preset(Task **tasks, size_t count, 
                                         const char *range_type,
                                         Task **filtered_tasks);

/**
 * Clean up task manager resources
 * @param tasks Task array to free
 * @param count Number of tasks
 */
void task_manager_cleanup(Task **tasks, size_t count);

#endif /* TASK_MANAGER_H */
