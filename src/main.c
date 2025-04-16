#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <curses.h>
#include "ai_assist.h"
#include "ui.h"
#include "storage.h"
#include "task.h"

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

// Parse date in YYYY-MM-DD to time_t (midnight UTC), 0 if empty/invalid
static time_t parse_date(const char *s) {
    if (!s || s[0] == '\0') return 0;
    struct tm tm = {0};
    if (!strptime(s, "%Y-%m-%d", &tm)) return 0;
    // Set midnight
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    return timegm(&tm);
}

int main(int argc, char *argv[]) {
    if (argc >= 3 && strcmp(argv[1], "ai-add") == 0) {
        ai_smart_add_default(argv[2]);
        return 0;
    }

    // Initialize storage
    if (storage_init() != 0) {
        fprintf(stderr, "Failed to initialize storage.\n");
        return 1;
    }

    // Load tasks
    size_t count = 0;
    Task **tasks = storage_load_tasks(&count);
    if (!tasks) {
        fprintf(stderr, "Failed to load tasks.\n");
        return 1;
    }

    // Initialize UI
    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize UI.\n");
        storage_free_tasks(tasks, count);
        return 1;
    }

    size_t selected = 0;
    int sort_mode = BY_CREATION;
    char search_term[64] = "";

    while (1) {
        // Build display list with search/filter
        Task **disp = malloc((count + 1) * sizeof(Task*));
        size_t disp_count = 0;
        for (size_t i = 0; i < count; ++i) {
            if (search_term[0] == '\0' || task_matches_search(tasks[i], search_term)) {
                disp[disp_count++] = tasks[i];
            }
        }
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
                time_t due = parse_date(date_str);
                // Create and append
                Task *t = task_create(name, due, (const char **)tag_tokens, tag_count, prio);
                tasks = realloc(tasks, (count + 1 + 1) * sizeof(Task*));
                tasks[count++] = t;
                tasks[count] = NULL;
                // Sort
                if (sort_mode == BY_NAME)
                    qsort(tasks, count, sizeof(Task*), task_compare_by_name);
                else
                    qsort(tasks, count, sizeof(Task*), task_compare_by_due);
                break;
            }
            case 'd': {
                if (disp_count == 0) break;
                Task *t = disp[selected];
                // Find in tasks
                for (size_t i = 0; i < count; ++i) {
                    if (tasks[i] == t) {
                        task_free(t);
                        // Shift
                        memmove(&tasks[i], &tasks[i+1], (count - i) * sizeof(Task*));
                        count--;
                        tasks = realloc(tasks, (count + 1) * sizeof(Task*));
                        tasks[count] = NULL;
                        break;
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

                // Update fields if changed
                if (name[0] != '\0') {
                    free(t->name);
                    t->name = strdup(name);
                }
                if (date_str[0] != '\0') {
                    t->due = parse_date(date_str);
                }
                // Tags
                for (size_t i = 0; i < t->tag_count; ++i) free(t->tags[i]);
                t->tag_count = 0;
                if (tags_str[0] != '\0') {
                    char *tag_tokens[16];
                    size_t tag_count = 0;
                    char *tok = strtok(tags_str, ",");
                    while (tok && tag_count < 16) {
                        tag_tokens[tag_count++] = tok;
                        tok = strtok(NULL, ",");
                    }
                    t->tags = realloc(t->tags, tag_count * sizeof(char*));
                    for (size_t i = 0; i < tag_count; ++i) {
                        t->tags[i] = strdup(tag_tokens[i]);
                    }
                    t->tag_count = tag_count;
                } else {
                    free(t->tags);
                    t->tags = NULL;
                    t->tag_count = 0;
                }
                // Priority
                if (strcasecmp(prio_str, "high") == 0) t->priority = PRIORITY_HIGH;
                else if (strncasecmp(prio_str, "med", 3) == 0) t->priority = PRIORITY_MEDIUM;
                else t->priority = PRIORITY_LOW;
                break;
            }
            case 'm': {
                if (disp_count == 0) break;
                Task *t = disp[selected];
                t->status = (t->status == STATUS_DONE) ? STATUS_PENDING : STATUS_DONE;
                break;
            }
            case 's': {
                char opt[8];
                prompt_input("Sort by (n)ame or (d)ate:", opt, sizeof(opt));
                if (opt[0] == 'n' || opt[0] == 'N') {
                    sort_mode = BY_NAME;
                    qsort(tasks, count, sizeof(Task*), task_compare_by_name);
                } else if (opt[0] == 'd' || opt[0] == 'D') {
                    sort_mode = BY_CREATION;
                    qsort(tasks, count, sizeof(Task*), task_compare_by_due);
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
        storage_save_tasks(tasks, count);
        free(disp);
    }

    // Cleanup
    ui_teardown();
    storage_save_tasks(tasks, count);
    storage_free_tasks(tasks, count);
    return 0;
}
