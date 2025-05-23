/* ui.c */
#include "ui.h"
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

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
    mvprintw(y, 1, "a:Add e:Edit d:Delete m:Mark v:ViewNote n:EditNote s:Sort /:Search +:NewProj -:DelProj q:Quit");
    attroff(A_REVERSE);
}

// Draw footer with AI chat mode help keys
void ui_draw_ai_chat_footer(void) {
    int y = LINES - 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "AI Chat | j/k:Navigate h/l:Proj v:ViewNote n:EditNote +:NewProj -:DelProj Enter:Command m:Mark q:Quit");
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
        char status_brackets[4] = "[ ]";
        
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
        
        // Prepare note indicator
        char note_indicator[4] = "[ ]";
        if (t->note && t->note[0] != '\0') {
            note_indicator[1] = 'x';
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
            
            // Print note icon (if present)
            if (t->note && t->note[0] != '\0') {
                mvprintw(y, offsetx + 25, "(N)");
            } else {
                mvprintw(y, offsetx + 25, "   ");
            }
            
            // Print task name with color
            attron(COLOR_PAIR(cp));
            mvprintw(y, offsetx + 29, "%s", t->name);
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
            
            // Print note icon (if present)
            if (t->note && t->note[0] != '\0') {
                mvprintw(y, offsetx + 25, "(N)");
            } else {
                mvprintw(y, offsetx + 25, "   ");
            }
            
            // Print task name
            if (t->status != STATUS_DONE) {
                attron(COLOR_PAIR(cp));
            }
            mvprintw(y, offsetx + 29, "%s", t->name);
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

void ui_draw_note_view(const Task *task, int scroll_offset, bool *out_has_more_content, int y_base, int x_content_start, int max_width, int max_lines) {
    if (!task) return;

    attron(A_DIM);
    mvhline(y_base, PROJECT_COL_WIDTH + 1, ACS_HLINE, COLS - PROJECT_COL_WIDTH - 2);
    attroff(A_DIM);

    int current_y_for_drawing = y_base + 1;

    if (task->note && task->note[0] != '\0') {
        attron(A_BOLD);
        char header_buf[256];
        snprintf(header_buf, sizeof(header_buf), "Note for: %.*s", max_width - 12, task->name);
        mvprintw(current_y_for_drawing, PROJECT_COL_WIDTH + 1, "%s", header_buf);
        attroff(A_BOLD);

        if (scroll_offset > 0) {
            mvprintw(current_y_for_drawing, COLS - 20, "^ more (k)");
        }
        current_y_for_drawing++;
        
        const char *full_note_text = task->note;
        char *note_copy = strdup(full_note_text);
        char *line = strtok(note_copy, "\n");
        
        int current_line_number = 0;
        int lines_to_display_count = 0;
        // int max_visible_content_lines = max_lines; // Parameterized
        bool more_content_below_current_display = false;

        while(line != NULL && current_line_number < scroll_offset) {
            line = strtok(NULL, "\n");
            current_line_number++;
        }

        while (line != NULL && lines_to_display_count < max_lines) {
            const char *segment_to_print = line;
            while (strlen(segment_to_print) > 0) {
                if (lines_to_display_count >= max_lines) break;
                
                char sub_line_buf[max_width + 1];
                int chars_in_sub_line = 0;
                if ((int)strlen(segment_to_print) > max_width) {
                    strncpy(sub_line_buf, segment_to_print, max_width);
                    sub_line_buf[max_width] = '\0';
                    char *last_space = strrchr(sub_line_buf, ' ');
                    if (last_space && last_space != sub_line_buf) {
                        chars_in_sub_line = last_space - sub_line_buf;
                    } else {
                        chars_in_sub_line = max_width;
                    }
                } else {
                    chars_in_sub_line = strlen(segment_to_print);
                }
                
                strncpy(sub_line_buf, segment_to_print, chars_in_sub_line);
                sub_line_buf[chars_in_sub_line] = '\0';
                mvprintw(current_y_for_drawing + lines_to_display_count, x_content_start, "%s", sub_line_buf);
                lines_to_display_count++;
                segment_to_print += chars_in_sub_line;
                if (*segment_to_print == ' ') segment_to_print++;
            }
            line = strtok(NULL, "\n");
        }
        
        if (note_copy) {
            free(note_copy); 
        }
        note_copy = strdup(full_note_text);
        line = strtok(note_copy, "\n");
        current_line_number = 0;
        int total_original_lines_processed = 0;
        // Count lines displayed + lines scrolled past to see if original note has more lines
        while(line != NULL && total_original_lines_processed < (scroll_offset + lines_to_display_count) ) {
            // This logic is tricky: we need to see if there are more *original* lines *after* the ones that *would have formed* the 'lines_to_display_count' screen lines.
            // A simpler proxy: are there any original lines after scroll_offset + (number of original lines that contributed to lines_to_display_count)?
            // For now, let's re-evaluate based on actual lines from strtok after the current scroll_offset + lines_to_display_count (approx)
            // This needs to be more robust if a single original line wraps many times.
            line = strtok(NULL, "\n");
            total_original_lines_processed++; // This counts original lines
        }
         // A more direct check: after displaying 'lines_to_display_count' screen lines (which may come from few or many original lines),
         // are there more original lines in the note *after* the original lines we've already started processing (i.e., after scroll_offset)?
        char *check_copy = strdup(full_note_text);
        char *current_orig_line = strtok(check_copy, "\n");
        int orig_line_idx = 0;
        while(current_orig_line != NULL && orig_line_idx < scroll_offset) {
            current_orig_line = strtok(NULL, "\n");
            orig_line_idx++;
        }
        // Now current_orig_line is the first original line that *could* be displayed.
        // We need to see how many screen lines these *displayed* original lines took up.
        // And then check if there are more original lines *after* those.
        // This is complex. A simpler, though potentially less accurate for extreme wrapping, check:
        int displayed_original_lines_count = 0;
        char *temp_copy = strdup(full_note_text);
        char *temp_line = strtok(temp_copy, "\n");
        for(int i=0; i < scroll_offset && temp_line; ++i) temp_line = strtok(NULL, "\n"); // Skip scrolled lines
        
        int screen_lines_from_current_orig_lines = 0;
        while(temp_line && screen_lines_from_current_orig_lines < max_lines){
            const char* segment = temp_line;
            while(strlen(segment) > 0 && screen_lines_from_current_orig_lines < max_lines){
                screen_lines_from_current_orig_lines++;
                int len = strlen(segment);
                segment += (len > max_width) ? max_width : len;
                if(*segment == ' ') segment++;
            }
            temp_line = strtok(NULL, "\n");
            displayed_original_lines_count++;
            if(!temp_line && screen_lines_from_current_orig_lines < max_lines) break; // No more original lines
        }
        free(temp_copy);

        // Now check if there are original lines *after* these displayed_original_lines_count (from the scroll_offset point)
        temp_copy = strdup(full_note_text);
        temp_line = strtok(temp_copy, "\n");
        for(int i=0; i < (scroll_offset + displayed_original_lines_count) && temp_line; ++i) {
            temp_line = strtok(NULL, "\n");
        }
        if (temp_line != NULL) { // If there's an original line after the ones we displayed / accounted for
            more_content_below_current_display = true;
        }
        free(temp_copy);
        if (note_copy) {
            free(note_copy); // Free the second strdup
        }

        *out_has_more_content = more_content_below_current_display;
    
        attron(A_DIM);
        if (more_content_below_current_display) {
            mvprintw(current_y_for_drawing + max_lines, x_content_start, "v more (j)");
        } else if (scroll_offset > 0) {
            mvprintw(current_y_for_drawing + max_lines, x_content_start, "(j/k scroll, v hide)");
        } else {
            mvprintw(current_y_for_drawing + max_lines, x_content_start, "(v to hide note)");
        }
        attroff(A_DIM);

    } else { // Task has no note or note is empty
        attron(A_BOLD);
        char header_buf[256];
        snprintf(header_buf, sizeof(header_buf), "Note for: %.*s", max_width - 12, task->name);
        mvprintw(current_y_for_drawing, PROJECT_COL_WIDTH + 1, "%s", header_buf);
        attroff(A_BOLD);
        current_y_for_drawing++;
        mvprintw(current_y_for_drawing, x_content_start, "No note for this task. Press 'n' to add/edit.");
        *out_has_more_content = false; // No note, so no more content
    }
}

bool ui_handle_note_edit(WINDOW *win, const char *initial_note_content, char *edited_note_buffer, size_t buffer_size, const char *task_name) {
    if (!win || !edited_note_buffer || buffer_size == 0) return false;

    int parent_y, parent_x;
    getmaxyx(win, parent_y, parent_x);
    (void)parent_x; // Unused for now, but good to have

    int height = parent_y / 2; // Half of screen height
    if (height < 10) height = 10; // Minimum height
    int width = COLS - (PROJECT_COL_WIDTH * 2); // Centralized, decent width
    if (width < 40) width = 40; // Minimum width
    // Ensure width is even to avoid potential rendering issues
    if (width % 2 != 0) width--;
    int start_y = (parent_y - height) / 2;
    int start_x = (COLS - width) / 2;

    WINDOW *edit_win = newwin(height, width, start_y, start_x);
    keypad(edit_win, TRUE);
    // Draw the box border - this takes up the outermost row and column on each side
    box(edit_win, 0, 0);

    char buffer[buffer_size]; // Internal buffer for editing
    strncpy(buffer, initial_note_content ? initial_note_content : "", buffer_size -1);
    buffer[buffer_size -1] = '\0';
    int len = strlen(buffer);
    int cursor_pos = len;
    int scroll_top_line = 0; // Tracks the top visible line in the editor window

    bool editing = true;
    bool saved = false;

    while (editing) {
        werase(edit_win);
        // Draw the box border first
        box(edit_win, 0, 0);
        // Place title and footer inside the box
        mvwprintw(edit_win, 0, 2, "Editing Note for: %.*s", width - 4, task_name ? task_name : "Selected Task"); // Position title at x=2 inside the box
        // Display footer with character count
        mvwprintw(edit_win, height - 1, 2, "F1:Save | ESC:Cancel | %d/%d chars", len, buffer_size - 1); // Added character counter

        // Display text with scrolling
        int text_area_height = height - 2; // available lines for text inside box
        char *line_start = buffer;
        int line_count_total = 0;

        // First, count all lines to determine scroll_top_line validity
        char *temp_line_start = buffer;
        while(temp_line_start) {
            line_count_total++;
            temp_line_start = strchr(temp_line_start, '\n');
            if (temp_line_start) temp_line_start++; else break;
        }
        if (scroll_top_line > 0 && scroll_top_line >= line_count_total - (text_area_height-1) ) {
             scroll_top_line = line_count_total - (text_area_height-1);
             if (scroll_top_line < 0) scroll_top_line = 0;
        }

        // Display lines starting from scroll_top_line
        int lines_scrolled_past = 0;
        line_start = buffer;
        while(lines_scrolled_past < scroll_top_line && line_start) {
            line_start = strchr(line_start, '\n');
            if (line_start) line_start++; else break;
            lines_scrolled_past++;
        }
        
        int cursor_screen_y = -1, cursor_screen_x = -1;
        if(line_start) { // only if there's something to display after scrolling
            char *current_pos_in_buffer_for_display = line_start;
            for (int i = 0; i < text_area_height; ++i) {
                if (!current_pos_in_buffer_for_display) break;
                char *next_newline = strchr(current_pos_in_buffer_for_display, '\n');
                int line_len;
                if (next_newline) {
                    line_len = next_newline - current_pos_in_buffer_for_display;
                } else {
                    line_len = strlen(current_pos_in_buffer_for_display);
                }

                // Calculate cursor position relative to this displayed line
                int line_start_offset_in_buffer = current_pos_in_buffer_for_display - buffer;
                if (cursor_pos >= line_start_offset_in_buffer && cursor_pos <= line_start_offset_in_buffer + line_len) {
                    cursor_screen_y = i + 1; 
                    cursor_screen_x = (cursor_pos - line_start_offset_in_buffer) + 1; // Position cursor at x=1 inside the box
                }

                // Use a more careful line-by-line rendering approach with wrapping support
                int max_text_width = width - 3; // -2 for borders, -1 for safety
                char line_to_print[max_text_width + 1];
                int j = 0;
                
                // Check if this line needs wrapping (longer than available width)
                if (line_len > max_text_width && next_newline) {
                    // This is a long line that needs wrapping
                    // Only display up to max_text_width characters
                    for (; j < max_text_width && current_pos_in_buffer_for_display[j]; j++) {
                        line_to_print[j] = current_pos_in_buffer_for_display[j];
                    }
                } else {
                    // Normal line processing - display up to newline or end of string
                    for (; j < max_text_width && current_pos_in_buffer_for_display[j] && 
                           (next_newline == NULL || &current_pos_in_buffer_for_display[j] < next_newline); j++) {
                        line_to_print[j] = current_pos_in_buffer_for_display[j];
                    }
                }
                
                line_to_print[j] = '\0';
                mvwprintw(edit_win, i + 1, 1, "%s", line_to_print);
                current_pos_in_buffer_for_display = next_newline ? next_newline + 1 : NULL;
            }
        }
        
        // Place cursor
        if (cursor_screen_y != -1 && cursor_screen_x != -1) {
             wmove(edit_win, cursor_screen_y, cursor_screen_x);
        } else {
            if (cursor_pos > (int)strlen(buffer)) cursor_pos = (int)strlen(buffer);
            
            if ((int)strlen(buffer) == 0) {
                wmove(edit_win, 1, 1); // Position cursor at x=1 inside the box
            } else {
                // Fallback cursor position if not visible
                char* last_line_ptr = buffer;
                char* current_line_ptr = buffer;
                int line_idx = 0;
                int visible_lines_count = 0;
                char* display_line_start_ptr = buffer;
                for(int k=0; k<scroll_top_line; ++k) {
                    display_line_start_ptr = strchr(display_line_start_ptr, '\n');
                    if (display_line_start_ptr) display_line_start_ptr++; else break;
                }
                if (!display_line_start_ptr) display_line_start_ptr = buffer;

                current_line_ptr = display_line_start_ptr;
                for(line_idx = 0; current_line_ptr && *current_line_ptr && line_idx < text_area_height; ++line_idx) {
                    last_line_ptr = current_line_ptr;
                    visible_lines_count++;
                    current_line_ptr = strchr(current_line_ptr, '\n');
                    if (current_line_ptr) current_line_ptr++;
                }
                int last_visible_line_start_offset = last_line_ptr - buffer;
                int last_visible_line_len = 0;
                char* end_of_last_visible_line = strchr(last_line_ptr, '\n');
                if(end_of_last_visible_line) last_visible_line_len = end_of_last_visible_line - last_line_ptr;
                else last_visible_line_len = (int)strlen(last_line_ptr);

                if (cursor_pos >= last_visible_line_start_offset && cursor_pos <= last_visible_line_start_offset + last_visible_line_len) {
                     wmove(edit_win, visible_lines_count, (cursor_pos - last_visible_line_start_offset) + 1); // Position cursor at x=1 inside the box
                } else { 
                     wmove(edit_win, visible_lines_count, last_visible_line_len + 1); // Position cursor at x=1 inside the box
                }
            }
        }

        wrefresh(edit_win);
        int ch = wgetch(edit_win);

        switch (ch) {
            case KEY_F(1): // Save
                saved = true;
                editing = false;
                break;
            case 27: // ESC 
                editing = false;
                break;
            case KEY_BACKSPACE:
            case 127: // often backspace
            case 8:   // sometimes backspace
                if (cursor_pos > 0) {
                    memmove(&buffer[cursor_pos - 1], &buffer[cursor_pos], len - cursor_pos + 1);
                    len--;
                    cursor_pos--;
                }
                break;
            case KEY_ENTER:
            case '\n': // Corrected case statement
                if (len < (int)buffer_size - 2) { // space for newline and null terminator
                    memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], len - cursor_pos + 1);
                    buffer[cursor_pos] = '\n';
                    len++;
                    cursor_pos++;
                }
                break;
            // Basic cursor movement (within the line, not yet across lines)
            // This needs to be significantly improved to handle multiline text correctly.
            case KEY_LEFT:
                if (cursor_pos > 0) cursor_pos--;
                break;
            case KEY_RIGHT:
                if (cursor_pos < len) cursor_pos++;
                break;
            // Scrolling for editor content (very basic, needs improvement)
            case KEY_UP:
                if(scroll_top_line > 0) scroll_top_line--;
                // TODO: Adjust cursor_pos to be within the new view or move to prev line
                break;
            case KEY_DOWN:
                 // Only scroll down if there's content below the current view
                if (line_count_total > scroll_top_line + text_area_height) {
                    scroll_top_line++;
                }
                // TODO: Adjust cursor_pos to be within the new view or move to next line
                break;
            default:
                if (ch >= 32 && ch <= 126) { // Printable chars
                    if (len < (int)buffer_size - 2) {
                        memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], len - cursor_pos + 1);
                        buffer[cursor_pos] = ch;
                        len++;
                        cursor_pos++;
                    }
                }
                break;
        }
    }

    delwin(edit_win);
    // Crucially, refresh the parent window to clear the subwindow area
    // This might be done by the caller (main loop) anyway with clear() + draw all.
    // touchwin(win); // Mark window for refresh
    // wnoutrefresh(win); // Schedule refresh

    if (saved) {
        strncpy(edited_note_buffer, buffer, buffer_size -1);
        edited_note_buffer[buffer_size -1] = '\0';
    }
    return saved;
}

int ui_get_input(void) {
    return getch();
}
