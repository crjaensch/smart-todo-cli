#include "date_parser.h"
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

// Internal helper functions
static bool parse_relative_date(const char *input, int *days, int *hours, int *minutes);
static bool parse_absolute_date(const char *input, struct tm *tm);
static bool parse_time(const char *input, struct tm *tm);
static bool parse_weekday(const char *input, int *weekday);
static bool parse_month(const char *input, int *month);
static void skip_whitespace(const char **str);
static bool starts_with(const char *str, const char *prefix);

bool parse_natural_date(const char *input, time_t *result) {
    if (!input || !result) return false;
    
    // Get current time
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    
    // Initialize result with current time
    struct tm tm_result = tm_now;
    
    // Check for relative dates (e.g., "tomorrow", "in 2 days")
    int days_offset = 0, hours_offset = 0, minutes_offset = 0;
    if (parse_relative_date(input, &days_offset, &hours_offset, &minutes_offset)) {
        // Apply the offsets
        tm_result.tm_mday += days_offset;
        tm_result.tm_hour = hours_offset;
        tm_result.tm_min = minutes_offset;
        tm_result.tm_sec = 0;
        
        *result = mktime(&tm_result);
        return true;
    }
    
    // Check for absolute dates (e.g., "may 20", "next monday")
    if (parse_absolute_date(input, &tm_result)) {
        *result = mktime(&tm_result);
        return true;
    }
    
    // Try to parse as time only (e.g., "2pm", "14:30")
    if (parse_time(input, &tm_result)) {
        *result = mktime(&tm_result);
        return true;
    }
    
    return false;
}

bool parse_time_today(const char *time_str, time_t *result) {
    if (!time_str || !result) return false;
    
    // Get current time
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    
    // Try to parse the time
    if (!parse_time(time_str, &tm_now)) {
        return false;
    }
    
    *result = mktime(&tm_now);
    
    // If the time is in the past, assume it's for tomorrow
    if (*result < now) {
        *result += 24 * 60 * 60; // Add one day
    }
    
    return true;
}

const char *format_natural_date(time_t timestamp, char *buf, size_t buf_size) {
    if (!buf || buf_size < 1) return NULL;
    
    struct tm tm;
    localtime_r(&timestamp, &tm);
    
    // Format as "Today at 2:30 PM" or "Tomorrow at 10:00 AM" or "May 20 at 3:00 PM"
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    
    int days_diff = (tm.tm_yday - tm_now.tm_yday + 365) % 365;
    
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &tm);
    
    if (days_diff == 0) {
        snprintf(buf, buf_size, "Today at %s", time_buf);
    } else if (days_diff == 1) {
        snprintf(buf, buf_size, "Tomorrow at %s", time_buf);
    } else if (days_diff < 7) {
        char day_buf[16];
        strftime(day_buf, sizeof(day_buf), "%A", &tm);
        snprintf(buf, buf_size, "%s at %s", day_buf, time_buf);
    } else {
        char date_buf[32];
        strftime(date_buf, sizeof(date_buf), "%b %d", &tm);
        snprintf(buf, buf_size, "%s at %s", date_buf, time_buf);
    }
    
    return buf;
}

// Internal helper functions

static bool parse_relative_date(const char *input, int *days, int *hours, int *minutes) {
    if (!input) return false;
    
    const char *p = input;
    skip_whitespace(&p);
    
    // Check for "tomorrow"
    if (strncasecmp(p, "tomorrow", 8) == 0) {
        *days = 1;
        *hours = 9; // Default to 9 AM
        *minutes = 0;
        return true;
    }
    
    // Check for "in X (days|hours|minutes)"
    if (starts_with(p, "in ")) {
        p += 3;
        skip_whitespace(&p);
        
        // Parse number
        char *endptr;
        long num = strtol(p, &endptr, 10);
        if (endptr == p) return false; // No number found
        
        p = endptr;
        skip_whitespace(&p);
        
        // Parse time unit
        if (starts_with(p, "day") || *p == 'd') {
            *days = (int)num;
        } else if (starts_with(p, "hour") || *p == 'h') {
            *hours = (int)num;
        } else if (starts_with(p, "minute") || starts_with(p, "min") || *p == 'm') {
            *minutes = (int)num;
        } else {
            return false;
        }
        
        return true;
    }
    
    // Check for "next [weekday]"
    if (starts_with(p, "next ")) {
        p += 5;
        int weekday;
        if (parse_weekday(p, &weekday)) {
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            
            int days_until = (weekday - tm_now.tm_wday + 7) % 7;
            if (days_until == 0) days_until = 7; // Next week
            
            *days = days_until;
            *hours = 9; // Default to 9 AM
            *minutes = 0;
            return true;
        }
    }
    
    return false;
}

static bool parse_absolute_date(const char *input, struct tm *tm) {
    if (!input || !tm) return false;
    
    // Check for month names (e.g., "may 20")
    int month;
    if (parse_month(input, &month)) {
        const char *p = input;
        while (*p && !isdigit(*p)) p++;
        
        if (*p) {
            char *endptr;
            long day = strtol(p, &endptr, 10);
            if (day >= 1 && day <= 31) {
                // Found a valid month and day
                tm->tm_mon = month;
                tm->tm_mday = (int)day;
                
                // If the date is in the past, assume next year
                struct tm tm_copy = *tm;
                time_t t = mktime(&tm_copy);
                time_t now = time(NULL);
                
                if (t < now) {
                    tm->tm_year++;
                }
                
                // Default to 9 AM
                tm->tm_hour = 9;
                tm->tm_min = 0;
                tm->tm_sec = 0;
                
                return true;
            }
        }
    }
    
    // Check for weekdays (e.g., "monday")
    int weekday;
    if (parse_weekday(input, &weekday)) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        int days_until = (weekday - tm_now.tm_wday + 7) % 7;
        if (days_until == 0) days_until = 7; // Next week
        
        tm->tm_mday += days_until;
        tm->tm_hour = 9; // Default to 9 AM
        tm->tm_min = 0;
        tm->tm_sec = 0;
        
        return true;
    }
    
    return false;
}

static bool parse_time(const char *input, struct tm *tm) {
    if (!input || !tm) return false;
    
    const char *p = input;
    skip_whitespace(&p);
    
    // Parse hours
    char *endptr;
    long hours = strtol(p, &endptr, 10);
    if (endptr == p) return false; // No number found
    
    p = endptr;
    
    // Check for AM/PM
    bool is_pm = false;
    bool has_am_pm = false;
    
    skip_whitespace(&p);
    
    if (*p == ':') {
        p++;
        // Parse minutes
        long minutes = strtol(p, &endptr, 10);
        if (endptr == p) return false; // No number after colon
        
        p = endptr;
        tm->tm_min = (int)minutes;
    } else {
        tm->tm_min = 0;
    }
    
    skip_whitespace(&p);
    
    // Check for AM/PM
    if (toupper(*p) == 'A' || toupper(*p) == 'P') {
        is_pm = (toupper(*p) == 'P');
        has_am_pm = true;
        p++;
        if (toupper(*p) == 'M') p++;
    }
    
    // Adjust hours for 12-hour format
    if (has_am_pm) {
        if (hours == 12) {
            hours = is_pm ? 12 : 0;
        } else if (is_pm) {
            hours += 12;
        }
    } else if (hours >= 0 && hours < 12) {
        // Default to PM if no AM/PM specified and hours < 12
        hours += 12;
    }
    
    if (hours < 0 || hours > 23) return false;
    if (tm->tm_min < 0 || tm->tm_min > 59) return false;
    
    tm->tm_hour = (int)hours;
    tm->tm_sec = 0;
    
    return true;
}

static bool parse_weekday(const char *input, int *weekday) {
    if (!input || !weekday) return false;
    
    const char *days[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
    
    for (int i = 0; i < 7; i++) {
        if (strncasecmp(input, days[i], 3) == 0) { // Match first 3 letters
            *weekday = i;
            return true;
        }
    }
    
    return false;
}

static bool parse_month(const char *input, int *month) {
    if (!input || !month) return false;
    
    const char *months[] = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };
    
    for (int i = 0; i < 12; i++) {
        if (strncasecmp(input, months[i], 3) == 0) { // Match first 3 letters
            *month = i;
            return true;
        }
    }
    
    return false;
}

static void skip_whitespace(const char **str) {
    if (!str || !*str) return;
    
    while (isspace(**str)) {
        (*str)++;
    }
}

static bool starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    size_t len = strlen(prefix);
    return strncasecmp(str, prefix, len) == 0;
}
