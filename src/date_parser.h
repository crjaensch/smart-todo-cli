#ifndef DATE_PARSER_H
#define DATE_PARSER_H

#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses a natural language date string into a time_t value
 * 
 * @param input The input string containing a natural language date (e.g., "tomorrow at 2pm", "next monday")
 * @param result Pointer to store the parsed time_t value
 * @return bool True if parsing was successful, false otherwise
 */
bool parse_natural_date(const char *input, time_t *result);

/**
 * @brief Parses a time string into a time_t value for today
 * 
 * @param time_str Time string (e.g., "2pm", "14:30")
 * @param result Pointer to store the parsed time_t value
 * @return bool True if parsing was successful, false otherwise
 */
bool parse_time_today(const char *time_str, time_t *result);

/**
 * @brief Gets a human-readable string representation of a time_t value
 * 
 * @param time The time to format
 * @param buf Buffer to store the formatted string
 * @param buf_size Size of the buffer
 * @return const char* Pointer to the formatted string, or NULL on error
 */
const char *format_natural_date(time_t time, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif // DATE_PARSER_H
