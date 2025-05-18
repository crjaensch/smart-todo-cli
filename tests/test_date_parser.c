#include "minunit.h"
#include "../src/date_parser.h"
#include "test_utils.h"
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>  // For printf

// Test counter
int tests_run = 0;

// Forward declarations for test functions
static char *test_parse_time_today(void);
static char *test_parse_natural_date(void);

// Helper function to run all tests
static char *all_tests(void) {
    mu_run_test(test_parse_time_today);
    mu_run_test(test_parse_natural_date);
    return 0;
}

static char *test_parse_time_today(void) {
    time_t result;
    
    // Test valid time formats
    mu_assert("parse_time_today(\"14:30\") failed", parse_time_today("14:30", &result));
    mu_assert("parse_time_today(\"2:30pm\") failed", parse_time_today("2:30pm", &result));
    mu_assert("parse_time_today(\"9:15am\") failed", parse_time_today("9:15am", &result));
    
    // Test invalid time formats
    mu_assert("Empty string should fail", !parse_time_today("", &result));
    mu_assert("NULL input should fail", !parse_time_today(NULL, &result));
    mu_assert("Invalid time should fail", !parse_time_today("25:00", &result));
    mu_assert("Invalid format should fail", !parse_time_today("12:00xx", &result));
    
    return 0;
}

static char *test_parse_natural_date(void) {
    time_t result;
    
    // Test relative dates
    mu_assert("parse_natural_date(\"tomorrow\") failed", 
              parse_natural_date("tomorrow", &result));
    
    // Test absolute dates (using a format the parser supports)
    mu_assert("parse_natural_date(\"Dec 25\") failed",
              parse_natural_date("Dec 25", &result));
    
    // Test date with time (using a format the parser supports)
    mu_assert("parse_natural_date(\"Dec 25 2pm\") failed",
              parse_natural_date("Dec 25 2pm", &result));
    
    // Test invalid dates
    mu_assert("Invalid date should fail", !parse_natural_date("not a date", &result));
    mu_assert("Empty string should fail", !parse_natural_date("", &result));
    mu_assert("NULL input should fail", !parse_natural_date(NULL, &result));
    
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter
    
    printf("Running date_parser tests...\n");
    
    char *result = all_tests();
    if (result != 0) {
        printf("TEST FAILED: %s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);
    
    return result != 0;
}
