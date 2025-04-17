#ifndef TODO_APP_TASK_H
#define TODO_APP_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// Constants
#define MAX_TAGS 5
#define MAX_TAG_LEN 20
#define MAX_TASK_SERIALIZE_LEN 512

// Priority levels for tasks
typedef enum {
    PRIORITY_LOW,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH
} Priority;

// Status of a task
typedef enum {
    STATUS_PENDING,
    STATUS_DONE
} Status;

// Task structure
typedef struct {
    char *id;            // Unique identifier (UUID string)
    char *name;          // Task description
    time_t created;      // Creation timestamp
    time_t due;          // Due date timestamp, or 0 if none
    char **tags;         // Array of tag strings
    size_t tag_count;    // Number of tags
    Priority priority;   // Priority level
    Status status;       // Pending or done
} Task;

// Create a new task
// name and tags are copied internally; due may be 0 for no date
Task *task_create(const char *name,
                  time_t due,
                  const char *tags[],
                  size_t tag_count,
                  Priority priority);

// Free a task and its internal allocations
void task_free(Task *task);

// Convert a task to a JSON string (caller must free returned string)
char *task_to_json(const Task *task);

// Parse a JSON string into a Task (returns NULL on error)
Task *task_from_json(const char *json_str);

// Comparison functions for sorting
int task_compare_by_name(const void *a, const void *b);
int task_compare_by_creation(const void *a, const void *b);
int task_compare_by_due(const void *a, const void *b);

// Utility for filtering: returns true if task has given tag
bool task_has_tag(const Task *task, const char *tag);

// Utility for filtering: returns true if task matches status
bool task_has_status(const Task *task, Status status);

// Utility for searching: returns true if name or tags contain substring
bool task_matches_search(const Task *task, const char *search_term);

#endif // TODO_APP_TASK_H
