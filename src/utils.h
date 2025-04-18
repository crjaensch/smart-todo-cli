#ifndef TODO_APP_UTILS_H
#define TODO_APP_UTILS_H

#include <time.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * Parse date in YYYY-MM-DD to time_t (midnight UTC)
 * @param s Date string in YYYY-MM-DD format
 * @return time_t timestamp, or 0 if empty/invalid
 */
time_t utils_parse_date(const char *s);

/**
 * Format time_t as ISO8601 string (YYYY-MM-DDT00:00:00Z)
 * @param t time_t timestamp
 * @return Allocated string with ISO8601 date (caller must free)
 */
char *utils_time_to_iso8601(time_t t);

/**
 * Parse ISO8601 string to time_t
 * @param s ISO8601 string (YYYY-MM-DDT00:00:00Z)
 * @return time_t timestamp
 */
time_t utils_iso8601_to_time(const char *s);

/**
 * Display a temporary message at the specified line
 * @param msg Message to display
 * @param line Line number (typically LINES-2)
 * @param seconds Seconds to display the message (0 for no auto-clear)
 */
void utils_show_message(const char *msg, int line, int seconds);

/**
 * Safe memory allocation with error checking
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *utils_malloc(size_t size);

/**
 * Safe calloc with error checking
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to allocated and zeroed memory, or NULL on failure
 */
void *utils_calloc(size_t nmemb, size_t size);

/**
 * Safe memory reallocation with error checking
 * @param ptr Pointer to previously allocated memory (can be NULL)
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *utils_realloc(void *ptr, size_t size);

/**
 * Safe string duplication with error checking
 * @param s String to duplicate
 * @return Newly allocated copy of string, or NULL on failure
 */
char *utils_strdup(const char *s);

/**
 * Safe file opening with error checking
 * @param path File path
 * @param mode File mode ("r", "w", etc.)
 * @return File handle, or NULL on failure
 */
FILE *utils_fopen(const char *path, const char *mode);

/**
 * Safe file closing that handles NULL
 * @param fp File pointer to close (can be NULL)
 */
void utils_fclose(FILE *fp);

#endif // TODO_APP_UTILS_H
