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
    init_pair(CP_APPROACH, COLOR_YELLOW, -1);
    init_pair(CP_FUTURE,   COLOR_GREEN,  -1);
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
    int w = COLS;
    attron(A_REVERSE);
    mvhline(0, 0, ' ', w);
    mvprintw(0, 1, "Todo App - %s", status_msg);
    attroff(A_REVERSE);
}

void ui_draw_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "a:Add e:Edit d:Delete m:Mark s:Sort /:Search q:Quit");
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
    for (size_t i = 0; i < count && i < start + maxy; ++i) {
        size_t idx = i;
        if (idx < start) continue;
        int y = 1 + (i - start);
        Task *t = tasks[idx];
        int cp = ui_color_for_due(t->due);
        if (idx == selected) attron(A_STANDOUT);
        attron(COLOR_PAIR(cp));
        // Prepare display strings for priority and due date
        const char *prio_str = (t->priority == PRIORITY_HIGH) ? "high" : (t->priority == PRIORITY_MEDIUM ? "med" : "low");
        char date_buf[16];
        if (t->due > 0) {
            struct tm tm;
            gmtime_r(&t->due, &tm);
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm);
        } else {
            strcpy(date_buf, "--");
        }
        // Display: date [prio] --- [ ] Task name
        mvprintw(y, 2, "%10s [%4s] --- [%c] %s", date_buf, prio_str, (t->status==STATUS_DONE?'x':' '), t->name);
        attroff(COLOR_PAIR(cp));
        if (idx == selected) attroff(A_STANDOUT);
    }
}

int ui_get_input(void) {
    return getch();
}
