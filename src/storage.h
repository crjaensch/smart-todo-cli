#ifndef TODO_APP_STORAGE_H
#define TODO_APP_STORAGE_H

#include <stddef.h>
#include "task.h"

/**
 * Ensure the storage directory (~/.todo-app) exists.
 * @return 0 on success, -1 on error.
 */
int storage_init(void);

/**
 * Load tasks from ~/.todo-app/tasks.json.
 * @param count[out] number of tasks loaded
 * @return NULL-terminated array of Task* on success, or NULL on error.
 *         Caller must free tasks via storage_free_tasks().
 */
Task **storage_load_tasks(size_t *count);

/**
 * Save an array of tasks to ~/.todo-app/tasks.json.
 * @param tasks NULL-terminated array of Task*
 * @param count number of tasks
 * @return 0 on success, -1 on error.
 */
int storage_save_tasks(Task **tasks, size_t count);

/**
 * Free an array of tasks previously returned by storage_load_tasks.
 * @param tasks NULL-terminated array of Task*
 * @param count number of tasks
 */
void storage_free_tasks(Task **tasks, size_t count);

/**
 * Save an array of projects to ~/.todo-app/projects.json.
 * @param projects array of project name strings
 * @param count number of projects
 * @return 0 on success, -1 on error.
 */
int storage_save_projects(char **projects, size_t count);

/**
 * Load projects from ~/.todo-app/projects.json.
 * @param projects_out[out] pointer to array of project name strings (allocated, must be freed by caller)
 * @return number of projects loaded
 */
size_t storage_load_projects(char ***projects_out);

#endif // TODO_APP_STORAGE_H
