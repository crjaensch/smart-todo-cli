/* ui.c */
#include "ui.h"
#include <ncurses.h>
#include <time.h>
#include <string.h>

// Threshold for approaching tasks (e.g. 3 days)
static const time_t APPROACH_THRESH = 3 * 24 * 60 * 60;

int ui_init(void) {
    initscr();
    if (!has_colors()) return -1;
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT,  -1, -1);
    init_pair(CP_OVERDUE,  COLOR_RED,    -1);
    init_pair(CP_APPROACH, COLOR_CYAN, -1);
    init_pair(CP_FUTURE,   COLOR_CYAN,  -1);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    return 0;
}

void ui_teardown(void) {
    endwin();
}

void ui_draw_header(const char *status_msg) {
    // Suppress unused parameter warning
    (void)status_msg;
    
    int w = COLS;
    attron(A_REVERSE);
    mvhline(0, 0, ' ', w);
    
    // Add color for the smartodo text
    attron(COLOR_PAIR(CP_FUTURE));
    mvprintw(0, 1, "smartodo");
    attroff(COLOR_PAIR(CP_FUTURE));
    
    // Add the subtitle in normal color
    attron(A_DIM);
    mvprintw(0, 10, "- Terminal Smart Planner");
    attroff(A_DIM);
    
    // Add AI icon on the right side - using ASCII compatible version
    attron(COLOR_PAIR(CP_APPROACH));
    mvprintw(0, w - 7, "[AI]");
    attroff(COLOR_PAIR(CP_APPROACH));
    
    attroff(A_REVERSE);
}

void ui_draw_footer(void) {
    ui_draw_standard_footer();
}

void ui_draw_standard_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "a:Add e:Edit d:Delete m:Mark s:Sort /:Search q:Quit");
    attroff(A_REVERSE);
}

// Draw footer with AI chat mode help keys
void ui_draw_ai_chat_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "AI Chat | j/k:Navigate Enter:Command m:Mark q:Quit");
    attroff(A_REVERSE);
}

int ui_color_for_due(time_t due) {
    if (due == 0) return CP_FUTURE;
    time_t now = time(NULL);
    if (due < now) return CP_OVERDUE;
    if ((due - now) <= APPROACH_THRESH) return CP_APPROACH;
    return CP_FUTURE;
}

void ui_draw_tasks(Task **tasks, size_t count, size_t selected) {
    int maxy = LINES - 2; // excluding header/footer
    size_t start = 0;
    if (selected >= (size_t)maxy) start = selected - maxy + 1;
    
    // Clear the task display area
    for (int i = 1; i < LINES - 1; i++) {
        move(i, 0);
        clrtoeol();
    }
    
    for (size_t i = 0; i < count && i < start + maxy; ++i) {
        size_t idx = i;
        if (idx < start) continue;
        int y = 1 + (i - start);
        Task *t = tasks[idx];
        
        // Format priority
        const char *prio_str = "";
        if (t->priority == PRIORITY_HIGH) {
            prio_str = "high";
        } else if (t->priority == PRIORITY_MEDIUM) {
            prio_str = "med";
        } else {
            prio_str = "low";
        }
        
        // Format due date
        char due_str[16] = "--";
        if (t->due > 0) {
            struct tm tm_due;
            gmtime_r(&t->due, &tm_due);
            strftime(due_str, sizeof(due_str), "%Y-%m-%d", &tm_due);
        }
        
        // Determine task status indicator
        char status_brackets[3] = "[ ]";
        
        if (t->status == STATUS_DONE) {
            status_brackets[0] = '[';
            status_brackets[1] = 'x';
            status_brackets[2] = ']';
        } else if (t->priority == PRIORITY_HIGH) {
            status_brackets[0] = '[';
            status_brackets[1] = '!';
            status_brackets[2] = ']';
        } else {
            status_brackets[0] = '[';
            status_brackets[1] = ' ';
            status_brackets[2] = ']';
        }
        
        // Prepare task name with appropriate color
        int cp = ui_color_for_due(t->due);
        
        // Highlight selected task
        if (idx == selected) {
            attron(A_BOLD);
            
            // Print priority with the same color as due date
            attron(COLOR_PAIR(cp));
            mvprintw(y, 2, "[%s]", prio_str);
            attroff(COLOR_PAIR(cp));
            
            // Print separator
            mvprintw(y, 8, "::");
            
            // Print date with color
            attron(COLOR_PAIR(cp));
            mvprintw(y, 11, "%s", due_str);
            attroff(COLOR_PAIR(cp));
            
            // Print separator
            mvprintw(y, 21, "::");
            
            // Print status indicator
            mvprintw(y, 24, "%s", status_brackets);
            
            // Print task name with color
            attron(COLOR_PAIR(cp));
            mvprintw(y, 29, "%s", t->name);
            attroff(COLOR_PAIR(cp));
            
            attroff(A_BOLD);
        } else {
            // Apply dimming for completed tasks
            if (t->status == STATUS_DONE) {
                attron(A_DIM);
            }
            
            // Print priority with the same color as due date
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, 2, "[%s]", prio_str);
            if (t->status != STATUS_DONE) {
                attroff(COLOR_PAIR(cp));
            }
            
            // Print separator
            mvprintw(y, 8, "::");
            
            // Print date with color
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, 11, "%s", due_str);
            if (t->status != STATUS_DONE) {
                attroff(COLOR_PAIR(cp));
            }
            
            // Print separator
            mvprintw(y, 21, "::");
            
            // Print status indicator
            mvprintw(y, 24, "%s", status_brackets);
            
            // Print task name
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, 29, "%s", t->name);
            if (t->status != STATUS_DONE) {
                attroff(COLOR_PAIR(cp));
            }
            
            // Remove dimming if applied
            if (t->status == STATUS_DONE) {
                attroff(A_DIM);
            }
        }
    }
}

/**
 * Display a suggestion for the current task
 * @param y The y-coordinate to display the suggestion at
 * @param suggestion The suggestion text to display
 */
void ui_draw_suggestion(int y, const char *suggestion) {
    if (!suggestion || !suggestion[0]) return;
    
    // Draw the arrow indicator using ASCII alternative
    attron(COLOR_PAIR(CP_APPROACH));
    mvprintw(y, 2, "->" ); // ASCII arrow instead of Unicode
    attroff(COLOR_PAIR(CP_APPROACH));
    
    // Draw the "Suggest:" prefix
    attron(COLOR_PAIR(CP_APPROACH));
    mvprintw(y, 5, "Suggest:");
    attroff(COLOR_PAIR(CP_APPROACH));
    
    // Draw the suggestion text
    mvprintw(y, 15, "'%s", suggestion);
}

int ui_get_input(void) {
    return getch();
}
