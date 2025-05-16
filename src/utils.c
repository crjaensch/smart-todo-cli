#include "utils.h"
#include "date_parser.h"
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

// Parse date in various formats to time_t (midnight UTC), 0 if empty/invalid
time_t utils_parse_date(const char *s) {
    if (!s || s[0] == '\0') return 0;
    
    // First try natural language parsing
    time_t result = 0;
    if (parse_natural_date(s, &result)) {
        return result;
    }
    
    // Fall back to standard date formats
    struct tm tm = {0};
    
    // 1. ISO 8601 date with time (YYYY-MM-DDThh:mm:ssZ)
    if (strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
        return timegm(&tm);
    }
    
    // 2. ISO 8601 date only (YYYY-MM-DD)
    if (strptime(s, "%Y-%m-%d", &tm)) {
        // Set to midnight UTC
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        return timegm(&tm);
    }
    
    // 3. Try other common formats
    if (strptime(s, "%m/%d/%Y", &tm) || 
        strptime(s, "%d/%m/%Y", &tm) || 
        strptime(s, "%b %d, %Y", &tm) || 
        strptime(s, "%d %b %Y", &tm)) {
        // Set to midnight UTC
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        return timegm(&tm);
    }
    
    // No valid format found
    return 0;
}

// Format time_t as ISO8601 string
char *utils_time_to_iso8601(time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    char *buf = utils_malloc(21); // YYYY-MM-DDThh:mm:ssZ\0
    if (!buf) return NULL;
    strftime(buf, 21, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Parse ISO8601 to time_t
time_t utils_iso8601_to_time(const char *s) {
    struct tm tm = {0};
    strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return timegm(&tm);
}

// Display a temporary message at the specified line
void utils_show_message(const char *msg, int line, int seconds) {
    attron(A_REVERSE);
    mvhline(line, 0, ' ', COLS);
    mvprintw(line, 1, "%s", msg);
    attroff(A_REVERSE);
    refresh();
    
    // Only sleep if seconds > 0, otherwise just show the message without clearing
    if (seconds > 0) {
        sleep(seconds);
        mvhline(line, 0, ' ', COLS); // Clear the message
        refresh();
    }
}

// Safe memory allocation with error checking
void *utils_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        // Log error to stderr if allocation fails
        fprintf(stderr, "Memory allocation failed for %zu bytes\n", size);
    }
    return ptr;
}

// Safe calloc with error checking
void *utils_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        // Log error to stderr if allocation fails
        fprintf(stderr, "Memory allocation (calloc) failed for %zu elements of %zu bytes\n", nmemb, size);
    }
    return ptr;
}

// Safe memory reallocation with error checking
void *utils_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        // Log error to stderr if reallocation fails
        fprintf(stderr, "Memory reallocation failed for %zu bytes\n", size);
    }
    return new_ptr;
}

// Safe string duplication with error checking
char *utils_strdup(const char *s) {
    if (!s) return NULL;
    
    char *new_str = strdup(s);
    if (!new_str) {
        // Log error to stderr if duplication fails
        fprintf(stderr, "String duplication failed for %zu bytes\n", strlen(s) + 1);
    }
    return new_str;
}

// Safe file opening with error checking
FILE *utils_fopen(const char *path, const char *mode) {
    if (!path || !mode) return NULL;
    
    FILE *fp = fopen(path, mode);
    if (!fp) {
        // Log error to stderr if file opening fails
        fprintf(stderr, "Failed to open file '%s' with mode '%s': %s\n", 
                path, mode, strerror(errno));
    }
    return fp;
}

// Safe file closing that handles NULL
void utils_fclose(FILE *fp) {
    if (fp) {
        fclose(fp);
    }
}
