#include "ai_chat.h"
#include "llm_api.h"
#include "ui.h"         // For ncurses UI functions
#include "storage.h"    // For loading/saving tasks
#include "task.h"       // For task manipulation
#include <cjson/cJSON.h> // For parsing LLM response
#include <curses.h>    // For ncurses functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>    // For sleep (optional, for showing messages)

#define MAX_MSG_LEN 1024 // Increased buffer size
#define MAX_ERR_LEN 256

// --- Helper functions (partially adapted from main.c) ---

// Prompt user for input at bottom line (ncurses version)
static void prompt_input(const char *prompt, char *buf, size_t bufsize) {
    echo();
    nocbreak();
    int y = LINES - 2; // Line above the footer
    mvhline(y, 0, ' ', COLS); // Clear the line
    mvprintw(y, 1, "%s", prompt);
    clrtoeol(); // Clear to end of line
    move(y, (int)strlen(prompt) + 2);
    refresh(); // Show prompt before getting input
    getnstr(buf, (int)bufsize - 1);
    noecho();
    cbreak();
}

// Parse date in YYYY-MM-DD to time_t (midnight UTC), 0 if empty/invalid
static time_t parse_date(const char *s) {
    if (!s || s[0] == '\0') return 0;
    struct tm tm = {0};
    // Try ISO 8601 date first (more common)
    if (!strptime(s, "%Y-%m-%d", &tm)) {
        // Maybe just a relative term the LLM failed to convert?
        // For now, return 0, enhance later if needed.
        return 0;
    }
    // Set midnight UTC
    return timegm(&tm); // timegm assumes input is UTC
}

// Display a temporary message at the prompt line
static void show_message(const char *msg) {
    int y = LINES - 2;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "%s", msg);
    attroff(A_REVERSE);
    refresh();
    sleep(2); // Show message for 2 seconds
    mvhline(y, 0, ' ', COLS); // Clear the message
    refresh();
}

// Helper to build the system prompt string
static void build_system_prompt(char *prompt_buf, size_t buf_size, Task **tasks, size_t count) {
    char *ptr = prompt_buf;
    size_t remaining_size = buf_size;
    int written;

    // Use snprintf for safe concatenation, checking return values
    written = snprintf(ptr, remaining_size,
             "Persona: You are Smartodo, a specialized AI assistant for a command-line todo application.\n"
             "\n"
             "Instructions:\n"
             "- Analyze the user request and the current task list.\n"
             "- Today's date is %s UTC.\n"
             "- Output exactly one JSON object: {\"action\": \"ACTION_NAME\", \"params\": {PARAM_DICT}}. No extra text.\n"
             "- For actions targeting a specific task (mark, delete, edit), use the 'index' parameter, referring to the 1-based index shown in the 'Current Tasks' list.\n"
             "\n"
             "Supported Actions & Params:\n"
             " add_task: { \"name\": string, \"due\": \"YYYY-MM-DD\" | null, \"tags\": [string], \"priority\": \"low\"|\"medium\"|\"high\" }\n"
             " mark_done: { \"index\": number }\n"
             " delete_task: { \"index\": number }\n"
             " edit_task_status: { \"index\": number, \"status\": \"pending\"|\"done\" }\n" // Simplified edit
             " search_tasks: { \"term\": string | null } (null term clears search)\n"
             " sort_tasks: { \"by\": \"name\"|\"due\"|\"creation\" }\n"
             " list_tasks: {} (Use this if the user asks to see tasks, effectively clears search)\n"
             "\n"
             "Context:\n"
             "%s" // Task list context added here
             "\n"
             "Example (Add): User: \"add buy milk tomorrow high prio\" -> {\"action\":\"add_task\",\"params\":{\"name\":\"buy milk\",\"due\":\"YYYY-MM-DD\",\"tags\":[],\"priority\":\"high\"}}\n"
             "Example (Mark): User: \"mark item 2 done\" -> {\"action\":\"mark_done\",\"params\":{\"index\":2}}\n"
             "Example (Search): User: \"find tasks related to 'project x'\" -> {\"action\":\"search_tasks\",\"params\":{\"term\":\"project x\"}}\n"
             "Example (Clear Search): User: \"show all tasks\" -> {\"action\":\"list_tasks\",\"params\":{}}\n"
             "\n"
             "Now, process the user's request:\n",
             "%s", "%s");
    if (written < 0 || (size_t)written >= remaining_size) goto end_prompt; // Error or truncated
    ptr += written;
    remaining_size -= written;

    for (size_t i = 0; i < count; ++i) {
        if (!tasks[i]) continue; // Should not happen with NULL termination, but safe check
        
        // Use task_to_json which exists and returns allocated string
        char *task_json_str = task_to_json(tasks[i]);
        if (!task_json_str) continue; // Handle potential allocation failure

        // Append task JSON string safely
        written = snprintf(ptr, remaining_size, "%s\n", task_json_str);
        
        free(task_json_str); // Free the string returned by task_to_json

        if (written < 0 || (size_t)written >= remaining_size) goto end_prompt; // Error or truncated
        ptr += written;
        remaining_size -= written;
    }

    // Append the rest of the prompt safely
    snprintf(ptr, remaining_size,
            "---\n" \
            "Respond ONLY with a JSON object containing 'action' and parameters. " \
            "Valid actions: 'add_task' (name, due_date_iso?, tags?, priority?), 'mark_done' (index), 'delete_task' (index), 'edit_task' (index, name?, due_date_iso?, tags?, priority?), 'search_tasks' (term), 'sort_tasks' (field: name|due|prio), 'list_tasks' (no params), 'exit' (no params). " \
            "'index' is 1-based from the displayed list. Use ISO 8601 for dates (YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS). Priority: low, medium, high."
            );

end_prompt:
    // Ensure null termination even if truncated
    prompt_buf[buf_size - 1] = '\0';
}

// --- Main AI Chat REPL Function ---

int ai_chat_repl(void) {
    // Initialize storage
    if (storage_init() != 0) {
        fprintf(stderr, "Failed to initialize storage.\n");
        return 1;
    }

    // Load tasks
    size_t count = 0;
    Task **tasks = storage_load_tasks(&count);
    if (!tasks) {
        // Allow starting with an empty list if file doesn't exist/is empty
        tasks = calloc(1, sizeof(Task*)); // Need space for NULL terminator
        if (!tasks) {
            fprintf(stderr, "Failed to allocate initial task list.\n");
            return 1;
        }
        tasks[0] = NULL;
        count = 0;
        // Optional: Inform user if storage was newly created or empty
    }

    // Initialize UI
    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize UI.\n");
        storage_free_tasks(tasks, count);
        free(tasks); // Free the list itself
        return 1;
    }

    size_t selected = 0; // Keep track of selection for potential future use (e.g., showing context)
    char search_term[64] = ""; // For potential future search integration
    char last_error[MAX_ERR_LEN] = ""; // To display errors

    while (1) {
        // Build display list (currently includes all tasks)
        // In the future, this could be filtered based on LLM 'search' action
        Task **disp = malloc((count + 1) * sizeof(Task*));
        if (!disp) {
            // Handle allocation failure
            ui_teardown();
            storage_free_tasks(tasks, count);
            free(tasks);
            fprintf(stderr, "Failed to allocate memory for display list.\n");
            return 1;
        }
        
        size_t disp_count = 0;
        for (size_t i = 0; i < count; ++i) {
             if (search_term[0] == '\0' || task_matches_search(tasks[i], search_term)) {
                 disp[disp_count++] = tasks[i];
             }
        }
        disp[disp_count] = NULL; // Null terminate the display list
        if (selected >= disp_count && disp_count > 0) selected = disp_count - 1;
        if (disp_count == 0) selected = 0;

        // Draw UI
        clear();
        ui_draw_header(search_term[0] ? search_term : "AI Chat Mode");
        ui_draw_tasks(disp, disp_count, selected); // Pass 0 for selected initially
        // Modify footer for AI chat
        int foot_y = LINES - 1;
        attron(A_REVERSE);
        mvhline(foot_y, 0, ' ', COLS);
        mvprintw(foot_y, 1, "AI Chat | j/k:Navigate Enter:Command q:Quit");
        attroff(A_REVERSE);
        // Display last error if any
        if (last_error[0] != '\0') {
             int err_y = LINES - 2;
             attron(A_BOLD | COLOR_PAIR(CP_OVERDUE));
             mvprintw(err_y, 1, "Error: %s", last_error);
             attroff(A_BOLD | COLOR_PAIR(CP_OVERDUE));
             last_error[0] = '\0'; // Clear after displaying once
        }
        refresh();

        // Get user input - handle navigation keys first
        int ch = ui_get_input();
        if (ch == 'q') {
            free(disp); // Free display list before breaking
            break; // Exit loop
        }
        
        // Handle navigation keys
        switch (ch) {
            case KEY_DOWN:
            case 'j':
                if (selected + 1 < disp_count) selected++;
                free(disp); // Free display list before continuing
                continue; // Redraw UI with new selection
                
            case KEY_UP:
            case 'k':
                if (selected > 0) selected--;
                free(disp); // Free display list before continuing
                continue; // Redraw UI with new selection
                
            case 'm': {
                // Mark task as done/undone (toggle status)
                if (disp_count == 0) {
                    free(disp);
                    continue; // Nothing to mark, just redraw
                }
                Task *t = disp[selected];
                t->status = (t->status == STATUS_DONE) ? STATUS_PENDING : STATUS_DONE;
                show_message(t->status == STATUS_DONE ? "Task marked as done." : "Task marked as pending.");
                free(disp); // Free display list before continuing
                continue; // Redraw UI with updated status
            }
                
            case 10: // Enter key
                // Only proceed to AI chat when Enter is pressed
                break;
                
            default:
                // Ignore other keys, redraw
                free(disp); // Free display list before continuing
                continue;
        }
        
        // Get command input when Enter is pressed
        char user_input[MAX_MSG_LEN];
        prompt_input("Enter command:", user_input, sizeof(user_input));

        if (strcmp(user_input, "exit") == 0 || strlen(user_input) == 0) {
            free(disp); // Free display list before breaking
            break; // Exit loop
        }

        // ---- LLM Call, Parsing, and Action Execution ----
        last_error[0] = '\0'; // Clear previous error before new command

        // 1. Prepare System Prompt
        time_t now = time(NULL);
        struct tm tm_now;
        gmtime_r(&now, &tm_now);
        char today_str[20];
        strftime(today_str, sizeof(today_str), "%Y-%m-%d", &tm_now);

        // Build task list context string (show index, status, name)
        char task_context[8192] = "Current Tasks:\n"; // Be mindful of prompt size limits
        size_t context_len = strlen(task_context);
        if (disp_count == 0) {
             strcat(task_context, "(No tasks to display)\n");
        } else {
            for (size_t i = 0; i < disp_count; ++i) {
                char line_buf[256];
                snprintf(line_buf, sizeof(line_buf), "%zu: [%c] %s\n",
                         i + 1, // Display 1-based index to user/LLM
                         (disp[i]->status == STATUS_DONE ? 'x' : ' '),
                         disp[i]->name);
                if (context_len + strlen(line_buf) < sizeof(task_context) - 1) {
                    strcat(task_context, line_buf);
                    context_len += strlen(line_buf);
                } else {
                    strcat(task_context, "(...more tasks truncated...)\n");
                    break; // Avoid buffer overflow
                }
            }
        }

        char sys_prompt[4096]; // Increased size
        build_system_prompt(sys_prompt, sizeof(sys_prompt), disp, disp_count);

        // 2. Call LLM
        // NOTE: We are not using chat history here for simplicity, sending the full context each time.
        char *llm_response_json = NULL;
        // show_message("Thinking..."); // Optional feedback
        int llm_status = llm_chat(sys_prompt, user_input, &llm_response_json, 0); // temperature=0 for deterministic JSON

        if (llm_status != 0 || !llm_response_json || llm_response_json[0] == '\0') {
            snprintf(last_error, MAX_ERR_LEN, "AI interaction failed (status: %d)", llm_status);
            if (llm_response_json) free(llm_response_json);
            free(disp); // Free display list before continuing
            continue; // Go back to draw the error
        }

        // 3. Parse JSON Response - first extract content from OpenAI response
        cJSON *api_response = cJSON_Parse(llm_response_json);
        if (!api_response) {
            snprintf(last_error, MAX_ERR_LEN, "Failed to parse API response JSON");
            free(llm_response_json);
            free(disp);
            continue;
        }
        
        // Extract the content from the OpenAI response
        char *content = NULL;
        cJSON *choices = cJSON_GetObjectItemCaseSensitive(api_response, "choices");
        if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
            cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
            if (first_choice) {
                cJSON *message = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
                if (message) {
                    cJSON *content_item = cJSON_GetObjectItemCaseSensitive(message, "content");
                    if (content_item && cJSON_IsString(content_item)) {
                        content = strdup(content_item->valuestring);
                    }
                }
            }
        }
        
        // Free the original API response JSON
        free(llm_response_json);
        cJSON_Delete(api_response);
        
        if (!content) {
            snprintf(last_error, MAX_ERR_LEN, "Could not extract content from API response");
            free(disp);
            continue;
        }
        
        // Now parse the actual content as JSON
        cJSON *root = cJSON_Parse(content);
        free(content); // Free the extracted content string

        if (!root) {
            snprintf(last_error, MAX_ERR_LEN, "AI response was not valid JSON");
            free(disp); // Free display list before continuing
            continue;
        }

        cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "action");
        cJSON *params_item = cJSON_GetObjectItemCaseSensitive(root, "params");

        if (!cJSON_IsString(action_item) || !cJSON_IsObject(params_item)) {
            snprintf(last_error, MAX_ERR_LEN, "AI response JSON missing 'action' string or 'params' object.");
            cJSON_Delete(root);
            free(disp); // Free display list before continuing
            continue;
        }

        const char *action = action_item->valuestring;
        cJSON *params = params_item;

        // 4. Execute Action
        // We'll track success for potential future use, but we don't need to check it
        // at the end of the loop since we always free resources regardless of success

        if (strcmp(action, "add_task") == 0) {
            cJSON *name = cJSON_GetObjectItem(params, "name");
            cJSON *due = cJSON_GetObjectItem(params, "due"); // Can be string or null
            cJSON *tags = cJSON_GetObjectItem(params, "tags"); // Array
            cJSON *priority = cJSON_GetObjectItem(params, "priority"); // string

            // More lenient validation - only require name to be a valid string
            if (cJSON_IsString(name) && name->valuestring[0] != '\0') {
                // Parse Tags - handle missing or invalid tags array
                const char *tag_ptrs[16]; // Max 16 tags
                int tag_count = 0;
                
                // Only parse tags if they exist and are an array
                if (tags && cJSON_IsArray(tags)) {
                    cJSON *tag_elem;
                    cJSON_ArrayForEach(tag_elem, tags) {
                        if (cJSON_IsString(tag_elem) && tag_count < 16) {
                            tag_ptrs[tag_count++] = tag_elem->valuestring;
                        }
                    }
                }

                // Parse Priority - default to low if missing or invalid
                Priority prio = PRIORITY_LOW;
                if (priority && cJSON_IsString(priority)) {
                    if (strcasecmp(priority->valuestring, "high") == 0) prio = PRIORITY_HIGH;
                    else if (strcasecmp(priority->valuestring, "medium") == 0) prio = PRIORITY_MEDIUM;
                }

                // Parse Due Date - default to 0 (no due date) if missing or invalid
                time_t due_time = 0;
                if (due && cJSON_IsString(due)) {
                    due_time = parse_date(due->valuestring);
                }

                // Create Task
                Task *new_task = task_create(name->valuestring, due_time, tag_ptrs, tag_count, prio);
                if (new_task) {
                    // Allocate new array with space for the new task and NULL terminator
                    Task **new_tasks = malloc((count + 2) * sizeof(Task*));
                    if (new_tasks) {
                        // Copy existing tasks to new array
                        for (size_t i = 0; i < count; i++) {
                            new_tasks[i] = tasks[i];
                        }
                        new_tasks[count] = new_task;
                        new_tasks[count + 1] = NULL; // Keep list null-terminated
                        
                        // Free old array and update pointers
                        free(tasks);
                        tasks = new_tasks;
                        count++;
                        
                        show_message("Task added.");
                    } else {
                        // Allocation failed, clean up
                        task_free(new_task);
                        snprintf(last_error, MAX_ERR_LEN, "Error: Failed to allocate memory for new task.");
                        free(disp); // Free display list before continuing
                        continue;
                    }
                } else {
                     snprintf(last_error, MAX_ERR_LEN, "Failed to create task object.");
                     free(disp); // Free display list before continuing
                     continue;
                }
            } else {
                 snprintf(last_error, MAX_ERR_LEN, "Invalid params for add_task.");
                 free(disp); // Free display list before continuing
                 continue;
            }
        }
        else if (strcmp(action, "mark_done") == 0 || strcmp(action, "delete_task") == 0 || strcmp(action, "edit_task_status") == 0) {
             cJSON *index_item = cJSON_GetObjectItem(params, "index");
             if (cJSON_IsNumber(index_item)) {
                 int index_1based = (int)index_item->valuedouble;
                 if (index_1based >= 1 && (size_t)index_1based <= disp_count) {
                     size_t index_0based = index_1based - 1;
                     Task *target_task = disp[index_0based]; // Get task from the *displayed* list

                     if (strcmp(action, "mark_done") == 0) {
                         target_task->status = STATUS_DONE;
                         show_message("Task marked done.");
                     } else if (strcmp(action, "edit_task_status") == 0) {
                          cJSON *status_item = cJSON_GetObjectItem(params, "status");
                          if (cJSON_IsString(status_item)) {
                              if (strcmp(status_item->valuestring, "done") == 0) {
                                   target_task->status = STATUS_DONE;
                                   show_message("Task status updated.");
                              } else if (strcmp(status_item->valuestring, "pending") == 0) {
                                   target_task->status = STATUS_PENDING;
                                   show_message("Task status updated.");
                              } else {
                                   snprintf(last_error, MAX_ERR_LEN, "Invalid status value in edit_task_status.");
                                   free(disp); // Free display list before continuing
                                   continue;
                              }
                          } else {
                               snprintf(last_error, MAX_ERR_LEN, "Missing 'status' param for edit_task_status.");
                               free(disp); // Free display list before continuing
                               continue;
                          }
                     } else { // delete_task
                         // Find the task in the main 'tasks' list
                         size_t main_idx = (size_t)-1;
                         for (size_t i = 0; i < count; ++i) {
                              if (tasks[i] == target_task) {
                                   main_idx = i;
                                   break;
                              }
                         }
                         if (main_idx != (size_t)-1) {
                              // Free the task to be deleted
                              task_free(target_task);
                              
                              // Create a new array without the deleted task
                              Task **new_tasks = NULL;
                              if (count > 1) {
                                  new_tasks = malloc((count) * sizeof(Task*));
                                  if (!new_tasks) {
                                      snprintf(last_error, MAX_ERR_LEN, "Error: Failed to allocate memory for task list.");
                                      free(disp); // Free display list before continuing
                                      continue;
                                  }
                                  
                                  // Copy tasks, skipping the deleted one
                                  size_t new_idx = 0;
                                  for (size_t i = 0; i < count; i++) {
                                      if (i != main_idx) {
                                          new_tasks[new_idx++] = tasks[i];
                                      }
                                  }
                                  new_tasks[count-1] = NULL; // Null terminate
                              } else {
                                  // If this was the last task, just allocate an empty array
                                  new_tasks = calloc(1, sizeof(Task*));
                                  if (!new_tasks) {
                                      snprintf(last_error, MAX_ERR_LEN, "Error: Failed to allocate memory for empty task list.");
                                      free(disp); // Free display list before continuing
                                      continue;
                                  }
                                  new_tasks[0] = NULL; // Ensure null termination
                              }
                              
                              // Update task list and count
                              free(tasks);
                              tasks = new_tasks;
                              count--;
                              show_message("Task deleted.");
                         } else {
                              snprintf(last_error, MAX_ERR_LEN, "Task inconsistency: Cannot find displayed task in main list.");
                              free(disp); // Free display list before continuing
                              continue;
                         }
                     }
                 } else {
                     snprintf(last_error, MAX_ERR_LEN, "Invalid index %d provided (must be 1-%zu).", index_1based, disp_count);
                     free(disp); // Free display list before continuing
                     continue;
                 }
             } else {
                 snprintf(last_error, MAX_ERR_LEN, "Missing/invalid 'index' param for %s.", action);
                 free(disp); // Free display list before continuing
                 continue;
             }
        }
         else if (strcmp(action, "search_tasks") == 0) {
             cJSON *term_item = cJSON_GetObjectItem(params, "term");
             if (cJSON_IsString(term_item)) {
                 strncpy(search_term, term_item->valuestring, sizeof(search_term) - 1);
                 search_term[sizeof(search_term) - 1] = '\0'; // Ensure null termination
                 show_message("Search applied.");
             } else if (cJSON_IsNull(term_item)) {
                  search_term[0] = '\0'; // Clear search
                  show_message("Search cleared.");
             } else {
                  snprintf(last_error, MAX_ERR_LEN, "Invalid 'term' param for search_tasks.");
                  free(disp); // Free display list before continuing
                  continue;
             }
             selected = 0; // Reset selection on search change
         }
         else if (strcmp(action, "list_tasks") == 0) {
             search_term[0] = '\0'; // Clear search term
             selected = 0;
             show_message("Displaying all tasks.");
         }
         else if (strcmp(action, "sort_tasks") == 0) {
             cJSON *by_item = cJSON_GetObjectItem(params, "by");
             if (cJSON_IsString(by_item)) {
                 const char *sort_key = by_item->valuestring;
                 int (*compare_func)(const void *, const void *) = NULL;
                 if (strcmp(sort_key, "name") == 0) compare_func = task_compare_by_name;
                 else if (strcmp(sort_key, "due") == 0) compare_func = task_compare_by_due;
                 else if (strcmp(sort_key, "creation") == 0) compare_func = task_compare_by_creation;

                 if (compare_func) {
                     qsort(tasks, count, sizeof(Task*), compare_func);
                     show_message("Tasks sorted.");
                 } else {
                     snprintf(last_error, MAX_ERR_LEN, "Invalid 'by' value '%s' for sort_tasks.", sort_key);
                     cJSON_Delete(root);
                     free(disp); // Free display list before continuing
                     continue;
                 }
             } else {
                 snprintf(last_error, MAX_ERR_LEN, "Missing 'by' param for sort_tasks.");
                 cJSON_Delete(root);
                 free(disp); // Free display list before continuing
                 continue;
             }
         }
        else {
            snprintf(last_error, MAX_ERR_LEN, "Unknown action '%s' received from AI.", action);
            cJSON_Delete(root);
            free(disp); // Free display list before continuing
            continue;
        }

        cJSON_Delete(root); // Clean up JSON object
        free(disp); // Free display list at the end of each iteration
    }

    // Cleanup
    ui_teardown();

    // Save tasks before exiting
    if (storage_save_tasks(tasks, count) != 0) {
        // Don't print to stderr as ncurses is torn down. Maybe log?
    }

    storage_free_tasks(tasks, count);

    printf("Exiting AI chat mode.\n"); // Print after ncurses is done
    return 0;
}
