#ifndef TODO_APP_TASK_H
#define TODO_APP_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// Constants
#define MAX_TAGS 5
#define MAX_TAG_LEN 20
#define MAX_PROJECT_LEN 40
#define MAX_NOTE_LEN 512
#define MAX_TASK_SERIALIZE_LEN 1280

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
    char *project;       // Project name (defaults to "default")
    Priority priority;   // Priority level
    Status status;       // Pending or done
    char *note;          // Optional note for additional context
} Task;

// Function forward declarations
Task *task_create(const char *name,
                  time_t due,
                  const char *tags[],
                  size_t tag_count,
                  Priority priority);
void task_free(Task *task);
char *task_to_json(const Task *task);
Task *task_from_json(const char *json_str);
int task_compare_by_name(const void *a, const void *b);
int task_compare_by_creation(const void *a, const void *b);
int task_compare_by_due(const void *a, const void *b);
bool task_has_tag(const Task *task, const char *tag);
bool task_has_status(const Task *task, Status status);
bool task_matches_search(const Task *task, const char *search_term);

/**
 * Set a note for a task.
 * @param task The task to set the note for
 * @param note The note text (will be copied)
 * @return 0 on success, -1 on error
 */
int task_set_note(Task *task, const char *note);

/**
 * Get the note for a task.
 * @param task The task to get the note from
 * @return The note text or NULL if no note exists
 */
const char *task_get_note(const Task *task);

#endif // TODO_APP_TASK_H
