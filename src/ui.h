/* ui.h */
#ifndef TODO_APP_UI_H
#define TODO_APP_UI_H

#include <curses.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "task.h"

// Color pair definitions
#define CP_DEFAULT    1
#define CP_OVERDUE    2
#define CP_APPROACH   3
#define CP_FUTURE     4
#define CP_SELECTED_PROJECT 5  // selected-project highlight: black text on cyan background

/**
 * Initialize the TUI environment including colors and screen settings.
 * @return 0 on success, non-zero on failure
 */
int ui_init(void);

/**
 * Clean up the TUI environment and restore terminal settings.
 * Should be called before program exit.
 */
void ui_teardown(void);

/**
 * Draw the application header with status message or search prompt.
 * @param status_msg The message to display in the header area
 */
void ui_draw_header(const char *status_msg);

/**
 * Draw the list of tasks with appropriate highlighting based on due dates.
 * @param tasks Array of Task pointers to display
 * @param count Number of tasks in the array
 * @param selected Index of the currently selected task (highlighted)
 */
void ui_draw_tasks(Task **tasks, size_t count, size_t selected);

/**
 * Draw the application footer with help keys.
 * General purpose footer display function.
 */
void ui_draw_footer(void);

/**
 * Draw the application footer with standard mode help keys.
 * Used during normal task management operations.
 */
void ui_draw_standard_footer(void);

/**
 * Draw the application footer with AI chat mode help keys.
 * Used when in AI assistant chat mode.
 */
void ui_draw_ai_chat_footer(void);

/**
 * Draw a suggestion with an arrow indicator at the specified position.
 * @param y The vertical position (row) to draw the suggestion
 * @param suggestion The text of the suggestion to display
 */
void ui_draw_suggestion(int y, const char *suggestion);

/**
 * Handle user input and return the key code.
 * @return The key code of the pressed key
 */
int ui_get_input(void);

/**
 * Map a due time to an appropriate curses color pair.
 * @param due The due timestamp to evaluate
 * @return A curses color pair constant (CP_*)
 */
int ui_color_for_due(time_t due);

/**
 * Draw the list of projects in the sidebar with appropriate highlighting.
 * @param projects Array of project name strings
 * @param count Number of projects in the array
 * @param selected Index of the currently selected project (highlighted)
 */
void ui_draw_projects(char **projects, size_t count, size_t selected);

/**
 * Draw the note viewing area for a task with scrolling support.
 * @param task The task whose note should be displayed
 * @param scroll_offset Number of lines to scroll down from the beginning of the note
 * @param out_has_more_content Set to true if there is more content below the visible area
 * @param y_base The starting vertical position (row) for the note display
 * @param x_content_start The starting horizontal position (column) for the note content
 * @param max_width Maximum width of the note display area
 * @param max_lines Maximum number of lines to display
 */
void ui_draw_note_view(const Task *task, int scroll_offset, bool *out_has_more_content, int y_base, int x_content_start, int max_width, int max_lines);

/**
 * Handle the interactive note editing session in a popup window.
 * Provides a full-featured text editor with cursor movement, scrolling,
 * and character count display. Properly handles box borders and text positioning.
 * 
 * @param win Parent window for the editor
 * @param initial_note_content The note's current content to start editing with
 * @param edited_note_buffer Buffer to store the final edited note if saved
 * @param buffer_size Size of edited_note_buffer
 * @param task_name Name of the task being edited, for display in editor header
 * @return true if note was saved, false if cancelled
 */
bool ui_handle_note_edit(WINDOW *win, const char *initial_note_content, char *edited_note_buffer, size_t buffer_size, const char *task_name);

extern int PROJECT_COL_WIDTH;

#endif // TODO_APP_UI_H
