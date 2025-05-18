#include "test_utils.h"
#include <stdarg.h>
#include <stddef.h>  // For size_t

void utils_show_message(const char *format, ...) {
    (void)format;  // Mark as unused to suppress warning
    // No-op for tests
}

int utils_get_user_input(char *buffer, size_t size) {
    (void)buffer;  // Mark as unused
    (void)size;    // Mark as unused
    // No-op for tests
    return 0;
}
// Add any other utility function stubs that might be needed
