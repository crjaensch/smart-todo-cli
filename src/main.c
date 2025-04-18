#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <curses.h>
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

    // Initialize UI
    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize UI.\n");
        task_manager_cleanup(tasks, count);
        return 1;
    }

    size_t selected = 0;
    int sort_mode = BY_CREATION;
    char search_term[64] = "";

    while (1) {
        // Build display list with search/filter
        Task **disp = utils_malloc((count + 1) * sizeof(Task*));
        if (!disp) {
            ui_teardown();
            task_manager_cleanup(tasks, count);
            fprintf(stderr, "Failed to allocate memory for display list.\n");
            return 1;
        }
        
        size_t disp_count = task_manager_filter_by_search(tasks, count, search_term, disp);
        disp[disp_count] = NULL;
        
        if (selected >= disp_count && disp_count > 0) selected = disp_count - 1;

        // Draw UI
        clear();
        ui_draw_header(search_term[0] ? search_term : "All Tasks");
        ui_draw_tasks(disp, disp_count, selected);
        ui_draw_footer();
        refresh();

        int ch = ui_get_input();
        if (ch == 'q') break;
        switch (ch) {
            case KEY_DOWN:
            case 'j':
                if (selected + 1 < disp_count) selected++;
                break;
            case KEY_UP:
            case 'k':
                if (selected > 0) selected--;
                break;
            case 'a': {
                char name[128], date_str[16], tags_str[128], prio_str[8];
                prompt_input("Name:", name, sizeof(name));
                prompt_input("Due Date (YYYY-MM-DD, empty for none):", date_str, sizeof(date_str));
                prompt_input("Tags (comma-separated):", tags_str, sizeof(tags_str));
                prompt_input("Priority (low/med/high):", prio_str, sizeof(prio_str));
                
                // Parse tags
                char *tag_tokens[16];
                size_t tag_count = 0;
                char *tok = strtok(tags_str, ",");
                while (tok && tag_count < 16) {
                    tag_tokens[tag_count++] = tok;
                    tok = strtok(NULL, ",");
                }
                
                // Parse priority
                Priority prio = PRIORITY_LOW;
                if (strcasecmp(prio_str, "high") == 0) prio = PRIORITY_HIGH;
                else if (strncasecmp(prio_str, "med", 3) == 0) prio = PRIORITY_MEDIUM;
                
                // Parse due date
                time_t due = utils_parse_date(date_str);
                
                // Add task using task manager
                if (task_manager_add_task(&tasks, &count, name, due, (const char **)tag_tokens, tag_count, prio) != 0) {
                    mvprintw(LINES - 2, 1, "Failed to add task");
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
                break;
            }
            case 'd': {
                if (disp_count == 0) break;
                Task *t = disp[selected];
                
                // Find the task index in the original array
                size_t task_index = 0;
                for (; task_index < count; task_index++) {
                    if (tasks[task_index] == t) break;
                }
                
                // Delete the task if found
                if (task_index < count) {
                    if (task_manager_delete_task(&tasks, &count, task_index) != 0) {
                        mvprintw(LINES - 2, 1, "Failed to delete task");
                        clrtoeol();
                        refresh();
                        napms(1500);
                    }
                }
                
                if (selected > 0) selected--;
                break;
            }
            case 'e': {
                if (disp_count == 0) break;
                Task *t = disp[selected];
                char name[128], date_str[16], tags_str[128], prio_str[8];
                
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
                prompt_input("Edit Due Date (YYYY-MM-DD, empty for none):", date_str, sizeof(date_str));
                prompt_input("Edit Tags (comma-separated):", tags_str, sizeof(tags_str));
                prompt_input("Edit Priority (low/med/high):", prio_str, sizeof(prio_str));

                // Parse tags for update
                char *tag_tokens[16];
                size_t tag_count = 0;
                
                if (tags_str[0] != '\0') {
                    char *tok = strtok(tags_str, ",");
                    while (tok && tag_count < 16) {
                        tag_tokens[tag_count++] = tok;
                        tok = strtok(NULL, ",");
                    }
                }
                
                // Parse priority for update
                int prio = -1; // -1 means don't change
                if (strcasecmp(prio_str, "high") == 0) prio = PRIORITY_HIGH;
                else if (strncasecmp(prio_str, "med", 3) == 0) prio = PRIORITY_MEDIUM;
                else if (strcasecmp(prio_str, "low") == 0) prio = PRIORITY_LOW;
                
                // Parse due date for update
                time_t due = -1; // -1 means don't change
                if (date_str[0] != '\0') {
                    due = utils_parse_date(date_str);
                }
                
                // Update the task
                if (task_manager_update_task(t, name[0] ? name : NULL, 
                                           due, 
                                           tags_str[0] ? (const char **)tag_tokens : NULL, 
                                           tag_count,
                                           prio, -1) != 0) {
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
                break;
            }
            case 'm': {
                if (disp_count == 0) break;
                Task *t = disp[selected];
                task_manager_toggle_status(t);
                break;
            }
            case 's': {
                char opt[8];
                prompt_input("Sort by (n)ame or (d)ate:", opt, sizeof(opt));
                if (opt[0] == 'n' || opt[0] == 'N') {
                    sort_mode = BY_NAME;
                    task_manager_sort_by_name(tasks, count);
                } else if (opt[0] == 'd' || opt[0] == 'D') {
                    sort_mode = BY_CREATION;
                    task_manager_sort_by_due(tasks, count);
                }
                break;
            }
            case '/': {
                prompt_input("Search (empty to clear):", search_term, sizeof(search_term));
                selected = 0;
                break;
            }
            default:
                break;
        }
        
        // Save after any change
        task_manager_save_tasks(tasks, count);
        free(disp);
    }

    // Cleanup
    ui_teardown();
    task_manager_save_tasks(tasks, count);
    task_manager_cleanup(tasks, count);
    return 0;
}
