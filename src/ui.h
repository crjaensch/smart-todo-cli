/* ui.h */
#ifndef TODO_APP_UI_H
#define TODO_APP_UI_H

#include <stddef.h>
#include "task.h"

// Initialize the TUI; returns 0 on success
int ui_init(void);

// Clean up the TUI
void ui_teardown(void);

// Draw header: filter/search prompt or title
void ui_draw_header(const char *status_msg);

// Draw the list of tasks; highlights based on due date
// tasks: array of Task*, count: length
// selected: index of the currently selected task
void ui_draw_tasks(Task **tasks, size_t count, size_t selected);

// Draw footer with help keys
void ui_draw_footer(void);

// Handle user input; returns key code
int ui_get_input(void);

// Map due time to a curses color pair
int ui_color_for_due(time_t due);

#endif // TODO_APP_UI_H
