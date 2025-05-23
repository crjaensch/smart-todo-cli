#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <curses.h>
#include <ctype.h>
#include <stdbool.h>
#include "ai_assist.h"
#include "ui.h"
#include "storage.h"
#include "task.h"
#include "ai_chat.h"
#include "utils.h"
#include "task_manager.h"

// Sort modes
enum { BY_CREATION, BY_NAME } SortMode;

// Prompt user for input at bottom line
static void prompt_input(const char *prompt, char *buf, size_t bufsize) {
    echo();
    nocbreak();
    int y = LINES - 2;
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "%s", prompt);
    clrtoeol();
    move(y, (int)strlen(prompt) + 2);
    getnstr(buf, (int)bufsize - 1);
    noecho();
    cbreak();
}

// handle_project_left navigates to the previous project in the project list, wrapping around to the end if at the beginning.
static void handle_project_left(size_t *proj_selected, char **projects, const char **current_project, size_t *selected) {
    if (*proj_selected > 0) (*proj_selected)--;
    *current_project = projects[*proj_selected];
    *selected = 0;
}

// handle_project_right navigates to the next project in the project list, wrapping around to the beginning if at the end.
static void handle_project_right(size_t *proj_selected, char **projects, const char **current_project, size_t proj_count, size_t *selected) {
    if (*proj_selected + 1 < proj_count) (*proj_selected)++;
    *current_project = projects[*proj_selected];
    *selected = 0;
}

// handle_add_project prompts the user for a new project name and adds it to the task manager.
static void handle_add_project(size_t *proj_count, size_t *proj_selected, char ***projects, const char **current_project) {
    char proj_name[64];
    prompt_input("New project name:", proj_name, sizeof(proj_name));
    if (proj_name[0] != '\0') {
        if (task_manager_add_project(proj_name) == 0) {
            free(*projects);
            *proj_count = task_manager_get_projects(projects);
            *proj_selected = *proj_count - 1;
            *current_project = (*projects)[*proj_selected];
        } else {
            mvprintw(LINES - 2, 1, "Failed to add project");
            clrtoeol();
            refresh();
            napms(1500);
        }
    }
}

// handle_delete_project deletes the currently selected project, ensuring that the 'default' project and the last project cannot be deleted.
static void handle_delete_project(size_t proj_count, size_t *proj_selected, char **projects, const char **current_project, Task **tasks, size_t count) {
    if (proj_count <= 1) return; // Don't allow deleting last project
    const char *to_delete = projects[*proj_selected];
    if (strcmp(to_delete, "default") == 0) return; // Don't allow deleting default
    int del_result = task_manager_delete_project(to_delete, tasks, count);
    if (del_result == 0) {
        task_manager_save_projects();
        free(projects);
        proj_count = task_manager_get_projects(&projects);
        if (*proj_selected >= proj_count) *proj_selected = proj_count - 1;
        *current_project = projects[*proj_selected];
    } else {
        mvprintw(LINES - 2, 1, "Only projects without tasks can be deleted");
        clrtoeol();
        refresh();
        napms(1500);
    }
}

// handle_cursor_down moves the cursor down in the task list, stopping at the last item.
static void handle_cursor_down(size_t *selected, size_t disp_count) {
    if (*selected + 1 < disp_count) (*selected)++;
}

// handle_cursor_up moves the cursor up in the task list, stopping at the first item.
static void handle_cursor_up(size_t *selected) {
    if (*selected > 0) (*selected)--;
}

// handle_add_task prompts the user for task details and adds a new task to the task manager for the current project.
static void handle_add_task(Task ***tasks, size_t *count, const char *current_project) {
    char name[128], date_str[64], tags_str[128], prio_str[8], confirm[8];
    
    // Get task name
    prompt_input("Task name:", name, sizeof(name));
    if (name[0] == '\0') return; // User cancelled
    
    // Get and parse due date with natural language support
    bool date_valid = false;
    time_t due = 0;
    
    while (!date_valid) {
        prompt_input("Due (e.g., 'tomorrow 2pm', 'next monday', 'may 20'):", date_str, sizeof(date_str));
        
        if (date_str[0] == '\0') {
            // No due date
            date_valid = true;
            break;
        }
        
        // Try to parse the date
        due = utils_parse_date(date_str);
        
        if (due == 0) {
            // Invalid date format
            mvprintw(LINES - 2, 1, "Invalid date format. Try 'tomorrow', 'next monday', etc.");
            clrtoeol();
            refresh();
            napms(1500);
            continue;
        }
        
        // Show the parsed date for confirmation
        char formatted_date[64];
        struct tm tm_due;
        localtime_r(&due, &tm_due);
        strftime(formatted_date, sizeof(formatted_date), "%A, %B %d at %I:%M %p", &tm_due);
        
        snprintf(confirm, sizeof(confirm), "%s", "");
        mvprintw(LINES - 2, 1, "Due: %s. Okay? (Y/n): ", formatted_date);
        clrtoeol();
        refresh();
        
        // Get single character input
        noecho();
        cbreak();
        int ch = getch();
        echo();
        
        if (ch == 'n' || ch == 'N') {
            // User wants to re-enter the date
            continue;
        }
        
        // Date is valid and confirmed
        date_valid = true;
    }
    
    // Get tags
    prompt_input("Tags (comma-separated, optional):", tags_str, sizeof(tags_str));
    
    // Parse tags
    char *tag_tokens[16] = {0};
    size_t tag_count = 0;
    if (tags_str[0] != '\0') {
        char tags_copy[128];
        strncpy(tags_copy, tags_str, sizeof(tags_copy) - 1);
        tags_copy[sizeof(tags_copy) - 1] = '\0';
        
        char *saveptr;
        char *tok = strtok_r(tags_copy, ",", &saveptr);
        while (tok && tag_count < 16) {
            // Trim whitespace
            while (isspace(*tok)) tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && isspace(*end)) end--;
            *(end + 1) = '\0';
            
            if (*tok) {
                tag_tokens[tag_count] = strdup(tok);
                if (tag_tokens[tag_count]) {
                    tag_count++;
                }
            }
            tok = strtok_r(NULL, ",", &saveptr);
        }
    }
    
    // Get priority with natural language support
    Priority prio = PRIORITY_LOW;
    bool prio_valid = false;
    
    while (!prio_valid) {
        prompt_input("Priority (low/medium/high, default=low):", prio_str, sizeof(prio_str));
        
        if (prio_str[0] == '\0' || strncasecmp(prio_str, "low", 3) == 0) {
            prio = PRIORITY_LOW;
            prio_valid = true;
        } else if (strncasecmp(prio_str, "med", 3) == 0) {
            prio = PRIORITY_MEDIUM;
            prio_valid = true;
        } else if (strncasecmp(prio_str, "high", 4) == 0) {
            prio = PRIORITY_HIGH;
            prio_valid = true;
        } else {
            mvprintw(LINES - 2, 1, "Invalid priority. Use 'low', 'medium', or 'high'.");
            clrtoeol();
            refresh();
            napms(1500);
        }
    }
    
    // Add task using task manager
    if (task_manager_add_task(tasks, count, name, due, (const char **)tag_tokens, tag_count, prio, current_project) != 0) {
        mvprintw(LINES - 2, 1, "Failed to add task");
        clrtoeol();
        refresh();
        napms(1500);
    }
    
    // Free allocated tag strings
    for (size_t i = 0; i < tag_count; i++) {
        free(tag_tokens[i]);
    }
    
    // Sort tasks by due date
    task_manager_sort_by_due(*tasks, *count);
}

// handle_delete_task deletes the currently selected task from the task manager and adjusts the selection.
static void handle_delete_task(Task ***tasks, size_t *count, Task **disp, size_t disp_count, size_t *selected) {
    if (disp_count == 0) return;
    Task *t = disp[*selected];
    
    // Find the task index in the original array
    size_t task_index = 0;
    for (; task_index < *count; task_index++) {
        if ((*tasks)[task_index] == t) break;
    }
    
    // Delete the task if found
    if (task_index < *count) {
        if (task_manager_delete_task(tasks, count, task_index) != 0) {
            mvprintw(LINES - 2, 1, "Failed to delete task");
            clrtoeol();
            refresh();
            napms(1500);
        }
    }
    
    if (*selected > 0) (*selected)--;
}

// handle_edit_task allows the user to edit the currently selected task's details, updating the task manager accordingly.
static void handle_edit_task(Task **disp, size_t disp_count, size_t selected, int sort_mode, Task **tasks, size_t count) {
    if (disp_count == 0) return;
    Task *t = disp[selected];
    char name[128], date_str[64], tags_str[128], prio_str[8], edit_name[128];
    
    // Pre-fill with current values
    strncpy(name, t->name ? t->name : "", sizeof(name));
    if (t->due > 0) {
        struct tm tm;
        gmtime_r(&t->due, &tm);
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
    } else {
        date_str[0] = '\0';
    }
    
    tags_str[0] = '\0';
    for (size_t i = 0; i < t->tag_count; ++i) {
        strncat(tags_str, t->tags[i], sizeof(tags_str) - strlen(tags_str) - 1);
        if (i + 1 < t->tag_count) strncat(tags_str, ",", sizeof(tags_str) - strlen(tags_str) - 1);
    }
    
    const char *prio_val = (t->priority == PRIORITY_HIGH) ? "high" : (t->priority == PRIORITY_MEDIUM ? "medium" : "low");
    strncpy(prio_str, prio_val, sizeof(prio_str));

    // Prompt for edits
    prompt_input("Edit Name:", name, sizeof(name));
    // Edit name if provided
    if (name[0] != '\0') {
        strncpy(edit_name, name, sizeof(edit_name) - 1);
        edit_name[sizeof(edit_name) - 1] = '\0';
    } else {
        strncpy(edit_name, disp[selected]->name, sizeof(edit_name) - 1);
        edit_name[sizeof(edit_name) - 1] = '\0';
    }

    // Edit due date with natural language support
    bool date_valid = false;
    time_t new_due = -1; // -1 means don't change
    
    prompt_input("New due date (e.g., 'tomorrow 2pm', 'next monday', 'may 20', empty to keep):", 
                   date_str, sizeof(date_str));
    
    if (date_str[0] != '\0') {
        while (!date_valid) {
            // Try to parse the date
            new_due = utils_parse_date(date_str);
            
            if (new_due == 0) {
                // Invalid date format
                mvprintw(LINES - 2, 1, "Invalid date format. Try 'tomorrow', 'next monday', etc.");
                clrtoeol();
                refresh();
                napms(1500);
                prompt_input("New due date (e.g., 'tomorrow 2pm', 'next monday', 'may 20'):", 
                           date_str, sizeof(date_str));
                if (date_str[0] == '\0') break;
                continue;
            }
            
            // Show the parsed date for confirmation
            char formatted_date[64];
            struct tm tm_due;
            localtime_r(&new_due, &tm_due);
            strftime(formatted_date, sizeof(formatted_date), "%A, %B %d at %I:%M %p", &tm_due);
            
            mvprintw(LINES - 2, 1, "New due date: %s. Okay? (Y/n): ", formatted_date);
            clrtoeol();
            refresh();
            
            // Get single character input
            noecho();
            cbreak();
            int ch = getch();
            echo();
            
            if (ch == 'n' || ch == 'N') {
                // User wants to re-enter the date
                prompt_input("New due date (e.g., 'tomorrow 2pm', 'next monday', 'may 20'):", 
                           date_str, sizeof(date_str));
                if (date_str[0] == '\0') break;
                continue;
            }
            
            // Date is valid and confirmed
            date_valid = true;
        }
    }
    
    // Edit tags
    char new_tags_str[128] = "";
    prompt_input("Edit Tags (comma-separated, empty to keep):", new_tags_str, sizeof(new_tags_str));
    
    // Edit priority
    char new_prio_str[8] = "";
    prompt_input("Edit Priority (low/medium/high, empty to keep):", new_prio_str, sizeof(new_prio_str));
    
    // Parse priority
    Priority new_prio = disp[selected]->priority;
    if (new_prio_str[0] != '\0') {
        if (strncasecmp(new_prio_str, "high", 4) == 0) {
            new_prio = PRIORITY_HIGH;
        } else if (strncasecmp(new_prio_str, "med", 3) == 0) {
            new_prio = PRIORITY_MEDIUM;
        } else if (strncasecmp(new_prio_str, "low", 3) == 0) {
            new_prio = PRIORITY_LOW;
        }
    }

    // Parse tags for update
    char *tag_tokens[16] = {0};
    size_t tag_count = 0;
    
    if (new_tags_str[0] != '\0') {
        char tags_copy[128];
        strncpy(tags_copy, new_tags_str, sizeof(tags_copy) - 1);
        tags_copy[sizeof(tags_copy) - 1] = '\0';
        
        char *saveptr;
        char *tok = strtok_r(tags_copy, ",", &saveptr);
        while (tok && tag_count < 16) {
            // Trim whitespace
            while (isspace(*tok)) tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && isspace(*end)) end--;
            *(end + 1) = '\0';
            
            if (*tok) {
                tag_tokens[tag_count] = strdup(tok);
                if (tag_tokens[tag_count]) {
                    tag_count++;
                }
            }
            tok = strtok_r(NULL, ",", &saveptr);
        }
    }
    
    // Update the task using task manager
    int result = task_manager_update_task(
        disp[selected],
        edit_name,  // Use the edit_name which has either new name or existing name
        date_valid ? new_due : -1,
        tag_count > 0 ? (const char **)tag_tokens : NULL,
        tag_count,
        new_prio_str[0] ? new_prio : -1,
        -1  // status (don't change)
    );
    
    // Free allocated tag strings
    for (size_t i = 0; i < tag_count; i++) {
        free(tag_tokens[i]);
    }
    
    if (result != 0) {
        mvprintw(LINES - 2, 1, "Failed to update task");
        clrtoeol();
        refresh();
        napms(1500);
    }
    
    // Sort if needed
    if (sort_mode == BY_NAME) {
        task_manager_sort_by_name(tasks, count);
    } else {
        task_manager_sort_by_due(tasks, count);
    }
}

// handle_toggle_status toggles the status of the currently selected task (pending/done).
static void handle_toggle_status(Task **disp, size_t disp_count, size_t selected) {
    if (disp_count == 0) return;
    Task *t = disp[selected];
    task_manager_toggle_status(t);
}

// handle_sort_tasks prompts the user for a sorting criterion (name or date) and sorts the task list accordingly.
static void handle_sort_tasks(int *sort_mode, Task **tasks, size_t count) {
    char opt[8];
    prompt_input("Sort by (n)ame or (d)ate:", opt, sizeof(opt));
    if (opt[0] == 'n' || opt[0] == 'N') {
        *sort_mode = BY_NAME;
        task_manager_sort_by_name(tasks, count);
    } else if (opt[0] == 'd' || opt[0] == 'D') {
        *sort_mode = BY_CREATION;
        task_manager_sort_by_due(tasks, count);
    }
}

// handle_search_tasks prompts the user for a search term and filters the task list based on the search term.
static void handle_search_tasks(char *search_term, size_t term_size, size_t *selected) {
    prompt_input("Search (empty to clear):", search_term, term_size);
    *selected = 0;
}

// toggle_note_visibility toggles the visibility of the note for the currently selected task.
// Global variable to track note scrolling position
static int note_scroll_offset = 0;
static bool note_has_more_content_for_scrolling = false; // ADDED global flag

bool toggle_note_visibility(Task **disp, size_t disp_count, size_t selected, bool show_note) {
    if (disp_count == 0 || selected >= disp_count) {
        return false; // No task selected, can't view note
    }
    
    Task *task = disp[selected];
    if (!task) {
        return false; // Invalid task
    }
    
    // If the task has no note or we're already showing it, hide it
    if (!task->note || task->note[0] == '\0') {
        mvprintw(LINES - 2, 1, "This task has no note.");
        clrtoeol();
        refresh();
        napms(1500);
        return false;
    }
    
    // Reset scroll position when toggling note visibility
    note_scroll_offset = 0;
    
    // Toggle the note visibility
    return !show_note;
}

#define MAX_PROJECTS 64

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "ai-chat") == 0) {
        return ai_chat_repl();
    }
    if (argc >= 3 && strcmp(argv[1], "ai-add") == 0) {
        ai_smart_add_default(argv[2]);
        return 0;
    }

    // Initialize task manager
    if (task_manager_init() != 0) {
        fprintf(stderr, "Failed to initialize task manager.\n");
        return 1;
    }

    // Load tasks
    size_t count = 0;
    Task **tasks = task_manager_load_tasks(&count);
    if (!tasks) {
        fprintf(stderr, "Failed to load tasks.\n");
        return 1;
    }

    // Load projects
    task_manager_load_projects();

    // Build initial projects list from task manager
    char **projects = NULL;
    size_t proj_count = task_manager_get_projects(&projects);
    if (proj_count == 0) {
        task_manager_add_project("default");
        proj_count = task_manager_get_projects(&projects);
    }
    size_t proj_selected = 0;
    const char *current_project = projects[proj_selected];

    // Initialize UI
    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize UI.\n");
        task_manager_cleanup(tasks, count);
        free(projects);
        return 1;
    }

    size_t selected = 0;
    int sort_mode = BY_CREATION;
    char search_term[64] = "";
    bool show_note = false; // Track whether we're showing a note

    while (1) {
        // Build display list with search/filter
        Task **disp = utils_malloc((count + 1) * sizeof(Task*));
        if (!disp) {
            ui_teardown();
            task_manager_cleanup(tasks, count);
            free(projects);
            fprintf(stderr, "Failed to allocate memory for display list.\n");
            return 1;
        }
        
        // filter by current project first
        size_t tmp_count = task_manager_filter_by_project(tasks, count, current_project, disp);
        size_t disp_count = task_manager_filter_by_search(disp, tmp_count, search_term, disp);
        disp[disp_count] = NULL;
        
        if (selected >= disp_count && disp_count > 0) selected = disp_count - 1;

        // Draw UI
        clear();
        ui_draw_header(search_term[0] ? search_term : "All Tasks");
        ui_draw_projects(projects, proj_count, proj_selected);
        ui_draw_tasks(disp, disp_count, selected);
        
        // Display note if show_note is true and there's a selected task
        if (show_note && disp_count > 0 && selected < disp_count) {
            int note_area_height = 7; // 1 for separator, 1 for header, 5 for content
            int note_y_base = LINES - note_area_height - 1;
            int note_max_display_lines = 5;
            int note_max_width = COLS - PROJECT_COL_WIDTH - 4;
            int note_x_content_start = PROJECT_COL_WIDTH + 3;

            ui_draw_note_view(disp[selected], 
                              note_scroll_offset, 
                              &note_has_more_content_for_scrolling, 
                              note_y_base, 
                              note_x_content_start, 
                              note_max_width, 
                              note_max_display_lines);
        } else {
            // Note is hidden or no task selected, scroll position is reset in toggle_note_visibility
            // Ensure note_has_more_content_for_scrolling is false if note not shown
            if (!show_note) { // Explicitly set to false if show_note is false
                 note_has_more_content_for_scrolling = false;
            }
        }
        
        // Add a suggestion for the selected task if applicable (Restoring this section)
        if (disp_count > 0 && selected < disp_count) {
            Task *selected_task = disp[selected];
            char suggestion[128] = "";
            if (selected_task->status == STATUS_PENDING) {
                if (selected_task->due > 0) {
                    time_t now_time = time(NULL);
                    if (selected_task->due < now_time) {
                        strcpy(suggestion, "Mark as done or reschedule");
                    } else if (selected_task->priority == PRIORITY_HIGH) {
                        strcpy(suggestion, "Break into smaller steps");
                    }
                } else if (selected_task->priority == PRIORITY_LOW) {
                    strcpy(suggestion, "Set a due date");
                }
            } else if (selected_task->status == STATUS_DONE) {
                strcpy(suggestion, "Archive or delete");
            }
            if (suggestion[0] != '\0') {
                int suggestion_y = 2 + disp_count + 2; // Position below task list
                if (suggestion_y < LINES - 3) {
                    move(suggestion_y - 1, PROJECT_COL_WIDTH + 1);
                    clrtoeol();
                    move(suggestion_y, PROJECT_COL_WIDTH + 1);
                    clrtoeol();
                    ui_draw_suggestion(suggestion_y, suggestion);
                }
            }
        }
        
        ui_draw_standard_footer();
        refresh();

        int ch = ui_get_input();
        if (ch == 'q' || ch == 'Q') break;

        switch (ch) {
            case KEY_LEFT:
            case 'h':
                handle_project_left(&proj_selected, projects, &current_project, &selected);
                break;
            case KEY_RIGHT:
            case 'l':
                handle_project_right(&proj_selected, projects, &current_project, proj_count, &selected);
                break;
            case '+': 
                handle_add_project(&proj_count, &proj_selected, &projects, &current_project);
                break;
            case '-': 
                handle_delete_project(proj_count, &proj_selected, projects, &current_project, tasks, count);
                break;
            case KEY_DOWN:
            case 'j':
                 if (!show_note) { // Task navigation
                      handle_cursor_down(&selected, disp_count);
                      show_note = false; // ENSURE note is hidden after task navigation
                      note_scroll_offset = 0; // ENSURE scroll is reset
                  } else if (show_note && note_has_more_content_for_scrolling) {
                    note_scroll_offset++;
                  }
                break;
            case KEY_UP:
            case 'k':
                 if (!show_note) { // Task navigation
                     handle_cursor_up(&selected);
                     show_note = false; // ENSURE note is hidden after task navigation
                     note_scroll_offset = 0; // ENSURE scroll is reset
                 } else if (note_scroll_offset > 0) { // Note scrolling (show_note is true here)
                    note_scroll_offset--;
                }
                break;
            case 'a':
                handle_add_task(&tasks, &count, current_project);
                break;
            case 'd':
                handle_delete_task(&tasks, &count, disp, disp_count, &selected);
                break;
            case 'e':
                handle_edit_task(disp, disp_count, selected, sort_mode, tasks, count); // Corrected: removed & from tasks and count
                break;
            case 'm':
                handle_toggle_status(disp, disp_count, selected);
                break;
            case 's':
                handle_sort_tasks(&sort_mode, tasks, count);
                break;
            case '/':
                handle_search_tasks(search_term, sizeof(search_term), &selected);
                break;
            case 'v':
                show_note = toggle_note_visibility(disp, disp_count, selected, show_note);
                break;
            case 'N': 
            case 'n': 
                if (disp_count > 0 && selected < disp_count) {
                    char temp_note_buffer[MAX_NOTE_LEN];
                    const char* current_note = disp[selected]->note ? disp[selected]->note : "";
                    // strncpy is used to copy the note content.
                    // It's important to ensure null termination manually if the source string 
                    // is as long as or longer than the destination buffer minus one.
                    strncpy(temp_note_buffer, current_note, MAX_NOTE_LEN - 1);
                    temp_note_buffer[MAX_NOTE_LEN - 1] = '\0'; // Ensure null termination
    
                    // Call the new UI function for note editing
                    if (ui_handle_note_edit(stdscr, current_note, temp_note_buffer, MAX_NOTE_LEN, disp[selected]->name)) {
                        task_set_note(disp[selected], temp_note_buffer); // Save the note if changes were made
                    }
                }
                show_note = false; // Hide note view after editing session
                note_scroll_offset = 0; // Reset scroll offset for the note view
                break;
            case 'C': // AI Chat mode
                if (ai_chat_repl() == 1) { // Check for conventional return code 1 to exit main app
                    goto cleanup_and_exit; 
                }
                break;
            default:
                break;
        }
        
        task_manager_save_tasks(tasks, count);
        free(disp);
    }

cleanup_and_exit: // Label for AI chat to exit application
    ui_teardown();
    task_manager_save_tasks(tasks, count);
    task_manager_save_projects();
    task_manager_cleanup(tasks, count);
    for(size_t i=0; i<proj_count; ++i) free(projects[i]);
    free(projects);
    return 0;
}
