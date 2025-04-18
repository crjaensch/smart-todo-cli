#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <unistd.h>

// Parse date in various formats to time_t (midnight UTC), 0 if empty/invalid
time_t utils_parse_date(const char *s) {
    if (!s || s[0] == '\0') return 0;
    
    struct tm tm = {0};
    
    // Try different date formats in order of preference
    
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
    char *buf = malloc(21); // YYYY-MM-DDThh:mm:ssZ\0
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
