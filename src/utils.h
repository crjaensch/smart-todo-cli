#ifndef TODO_APP_UTILS_H
#define TODO_APP_UTILS_H

#include <time.h>
#include <stdbool.h>

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
 * @param seconds Seconds to display the message
 */
void utils_show_message(const char *msg, int line, int seconds);

#endif // TODO_APP_UTILS_H
