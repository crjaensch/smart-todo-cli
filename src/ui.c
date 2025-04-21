/* ui.c */
#include "ui.h"
#include <ncurses.h>
#include <time.h>
#include <string.h>

// Threshold for approaching tasks (e.g. 3 days)
static const time_t APPROACH_THRESH = 3 * 24 * 60 * 60;
int PROJECT_COL_WIDTH = 18;

int ui_init(void) {
    initscr();
    if (!has_colors()) return -1;
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT,  -1, -1);
    init_pair(CP_OVERDUE,  COLOR_RED,    -1);
    init_pair(CP_APPROACH, COLOR_CYAN, -1);
    init_pair(CP_FUTURE,   COLOR_CYAN,  -1);
    init_pair(CP_SELECTED_PROJECT, COLOR_BLACK, COLOR_CYAN);
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

    // Second header line: label columns
    attron(A_REVERSE);
    mvhline(1, 0, ' ', COLS);
    mvprintw(1, 1, "Projects");
    mvprintw(1, PROJECT_COL_WIDTH + 2, "Tasks");
    attroff(A_REVERSE);
}

void ui_draw_footer(void) {
    ui_draw_standard_footer();
}

void ui_draw_standard_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "a:Add e:Edit d:Delete m:Mark s:Sort /:Search +:NewProj -:DelProj q:Quit");
    attroff(A_REVERSE);
}

// Draw footer with AI chat mode help keys
void ui_draw_ai_chat_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "AI Chat | j/k:Navigate h/l:Proj +:NewProj -:DelProj Enter:Command m:Mark q:Quit");
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
    int maxy = LINES - 3; // excluding header/footer
    int offsetx = PROJECT_COL_WIDTH + 1;
    size_t start = 0;
    if (selected >= (size_t)maxy) start = selected - maxy + 1;
    
    // Clear the task display area
    for (int i = 2; i < LINES - 1; i++) {
        move(i, offsetx);
        clrtoeol();
    }
    
    for (size_t i = 0; i < count && i < start + maxy; ++i) {
        size_t idx = i;
        if (idx < start) continue;
        int y = 2 + (i - start);
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
            mvprintw(y, offsetx, "[%s]", prio_str);
            attroff(COLOR_PAIR(cp));
            
            // Print separator
            mvprintw(y, offsetx + 6, "::");
            
            // Print date with color
            attron(COLOR_PAIR(cp));
            mvprintw(y, offsetx + 9, "%s", due_str);
            attroff(COLOR_PAIR(cp));
            
            // Print separator
            mvprintw(y, offsetx + 19, "::");
            
            // Print status indicator
            mvprintw(y, offsetx + 22, "%s", status_brackets);
            
            // Print task name with color
            attron(COLOR_PAIR(cp));
            mvprintw(y, offsetx + 27, "%s", t->name);
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
            mvprintw(y, offsetx, "[%s]", prio_str);
            if (t->status != STATUS_DONE) {
                attroff(COLOR_PAIR(cp));
            }
            
            // Print separator
            mvprintw(y, offsetx + 6, "::");
            
            // Print date with color
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, offsetx + 9, "%s", due_str);
            if (t->status != STATUS_DONE) {
                attroff(COLOR_PAIR(cp));
            }
            
            // Print separator
            mvprintw(y, offsetx + 19, "::");
            
            // Print status indicator
            mvprintw(y, offsetx + 22, "%s", status_brackets);
            
            // Print task name
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, offsetx + 27, "%s", t->name);
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
    
    int offsetx = PROJECT_COL_WIDTH + 1;
    // Blank line above suggestion
    move(y - 1, offsetx);
    clrtoeol();
    mvaddch(y - 1, PROJECT_COL_WIDTH, ACS_VLINE);
    // Draw the arrow indicator using ASCII alternative
    attron(COLOR_PAIR(CP_APPROACH));
    mvprintw(y, offsetx, "->");
    attroff(COLOR_PAIR(CP_APPROACH));
    // Draw the "Suggest:" prefix
    attron(COLOR_PAIR(CP_APPROACH));
    mvprintw(y, offsetx + 3, "Suggest:");
    attroff(COLOR_PAIR(CP_APPROACH));
    // Draw the suggestion text
    mvprintw(y, offsetx + 15, "'%s", suggestion);
}

int ui_get_input(void) {
    return getch();
}

void ui_draw_projects(char **projects, size_t count, size_t selected) {
    // Clear column
    for (int i = 2; i < LINES - 1; ++i) {
        mvhline(i, 0, ' ', PROJECT_COL_WIDTH);
    }

    // Calculate max visible projects based on terminal height
    size_t max_visible = (size_t)(LINES - 3);
    
    // If we have more projects than can fit, ensure selected is visible
    size_t start_idx = 0;
    if (count > max_visible && selected >= max_visible) {
        start_idx = selected - max_visible + 1;
    }
    
    // Draw visible projects
    for (size_t i = start_idx; i < count && (i - start_idx) < max_visible; ++i) {
        int y_pos = 2 + (i - start_idx);
        
        if (i == selected) {
            attron(COLOR_PAIR(CP_SELECTED_PROJECT));
        }
        
        // Ensure we don't print beyond the column width
        mvprintw(y_pos, 1, "%.*s", PROJECT_COL_WIDTH - 2, projects[i]);
        
        if (i == selected) {
            attroff(COLOR_PAIR(CP_SELECTED_PROJECT));
        }
    }

    // vertical separator
    for (int y = 2; y < LINES - 1; ++y) mvaddch(y, PROJECT_COL_WIDTH, ACS_VLINE);
}
