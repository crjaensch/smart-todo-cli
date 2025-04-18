#include "ai_chat.h"
#include "llm_api.h"
#include "ui.h"         // For ncurses UI functions
#include "storage.h"    // For loading/saving tasks
#include "task.h"       // For task manipulation
#include "utils.h"      // For common utilities
#include <cjson/cJSON.h> // For parsing LLM response
#include <curses.h>    // For ncurses functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>    // For sleep (optional, for showing messages)

#define MAX_MSG_LEN 1024 // Increased buffer size
#define MAX_ERR_LEN 256

// --- Helper functions ---

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
             "- IMPORTANT: Only use task indices that are explicitly shown in the current task list. Do not reference tasks by absolute indices that may have changed.\n"
             "- To reference the currently selected task (marked with an arrow → and 'SELECTED' in the list), use the 'selected_task' action.\n"
             "\n"
             "Supported Actions & Params:\n"
             " add_task: { \"name\": string, \"due\": \"YYYY-MM-DD\" | null, \"tags\": [string], \"priority\": \"low\"|\"medium\"|\"high\" }\n"
             " mark_done: { \"index\": number }\n"
             " delete_task: { \"index\": number }\n"
             " edit_task: { \"index\": number, \"name\": string?, \"due\": \"YYYY-MM-DD\" | null?, \"tags\": [string]?, \"priority\": string?, \"status\": string? }\n"
             " selected_task: { \"action\": \"mark_done\" | \"delete_task\" | \"edit_task\", \"params\": {...} } (Apply an action to the currently selected task)\n"
             " search_tasks: { \"term\": string | null } (null term clears search)\n"
             " sort_tasks: { \"by\": \"name\"|\"due\"|\"creation\" }\n"
             " list_tasks: {} (Use this if the user asks to see tasks, effectively clears search)\n"
             " exit: {} (Use this to exit the AI chat mode)\n"
             "\n"
             "Context:\n"
             "%s" // Task list context added here
             "\n"
             "Example (Add): User: \"add buy milk tomorrow high prio\" -> {\"action\":\"add_task\",\"params\":{\"name\":\"buy milk\",\"due\":\"YYYY-MM-DD\",\"tags\":[],\"priority\":\"high\"}}\n"
             "Example (Mark): User: \"mark item 2 done\" -> {\"action\":\"mark_done\",\"params\":{\"index\":2}}\n"
             "Example (Edit): User: \"change due date of task 3 to next Friday\" -> {\"action\":\"edit_task\",\"params\":{\"index\":3,\"due\":\"YYYY-MM-DD\"}}\n"
             "Example (Selected): User: \"update the due date of the selected task to tomorrow\" -> {\"action\":\"selected_task\",\"params\":{\"action\":\"edit_task\",\"params\":{\"due\":\"YYYY-MM-DD\"}}}\n"
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
            "Valid actions: 'add_task' (name, due_date_iso?, tags?, priority?), 'mark_done' (index), 'delete_task' (index), 'edit_task' (index, name?, due_date_iso?, tags?, priority?), 'selected_task' (action, params), 'search_tasks' (term), 'sort_tasks' (field: name|due|prio), 'list_tasks' (no params), 'exit' (no params). " \
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
                utils_show_message(t->status == STATUS_DONE ? "Task marked as done." : "Task marked as pending.", LINES - 2, 2);
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
                char line_buf[512]; // Increased buffer size to accommodate more details
                char due_str[32] = "no due date";
                
                // Format due date if present
                if (disp[i]->due > 0) {
                    struct tm tm_due;
                    gmtime_r(&disp[i]->due, &tm_due);
                    strftime(due_str, sizeof(due_str), "%Y-%m-%d", &tm_due);
                }
                
                // Format priority
                const char *prio_str = "low";
                if (disp[i]->priority == PRIORITY_HIGH) prio_str = "high";
                else if (disp[i]->priority == PRIORITY_MEDIUM) prio_str = "medium";
                
                // Format tags
                char tags_str[256] = "";
                if (disp[i]->tag_count > 0) {
                    strcat(tags_str, "tags: ");
                    for (size_t j = 0; j < disp[i]->tag_count; ++j) {
                        if (j > 0) strcat(tags_str, ", ");
                        strncat(tags_str, disp[i]->tags[j], sizeof(tags_str) - strlen(tags_str) - 1);
                    }
                }
                
                // Create the full line with all task details
                snprintf(line_buf, sizeof(line_buf), "%zu: %s[%c] %s (due: %s, priority: %s%s%s)%s\n",
                         i + 1, // Display 1-based index to user/LLM
                         (i == selected) ? "→ " : "  ", // Add arrow indicator for selected task
                         (disp[i]->status == STATUS_DONE) ? 'x' : ' ',
                         disp[i]->name,
                         due_str,
                         prio_str,
                         (disp[i]->tag_count > 0) ? ", " : "",
                         tags_str,
                         (i == selected) ? " (SELECTED)" : ""); // Add SELECTED text for clarity
                         
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
                    due_time = utils_parse_date(due->valuestring);
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
                        
                        utils_show_message("Task added.", LINES - 2, 2);
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
                         utils_show_message("Task marked done.", LINES - 2, 2);
                     } else if (strcmp(action, "edit_task_status") == 0) {
                          cJSON *status_item = cJSON_GetObjectItem(params, "status");
                          if (cJSON_IsString(status_item)) {
                              if (strcasecmp(status_item->valuestring, "done") == 0) {
                                   target_task->status = STATUS_DONE;
                                   utils_show_message("Task status updated.", LINES - 2, 2);
                              } else if (strcasecmp(status_item->valuestring, "pending") == 0) {
                                   target_task->status = STATUS_PENDING;
                                   utils_show_message("Task status updated.", LINES - 2, 2);
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
                              utils_show_message("Task deleted.", LINES - 2, 2);
                         } else {
                              snprintf(last_error, MAX_ERR_LEN, "Task inconsistency: Cannot find displayed task in main list.");
                              free(disp); // Free display list before continuing
                              continue;
                         }
                     }
                 } else {
                     // Provide more detailed error message with available task indices
                     char index_error[MAX_ERR_LEN];
                     snprintf(index_error, MAX_ERR_LEN, 
                              "Invalid index %d provided. Available task indices are 1-%zu. Please check the task list and use a valid index.", 
                              index_1based, disp_count);
                     snprintf(last_error, MAX_ERR_LEN, "%s", index_error);
                     free(disp); // Free display list before continuing
                     continue;
                 }
             } else {
                 snprintf(last_error, MAX_ERR_LEN, "Missing/invalid 'index' param for %s.", action);
                 free(disp); // Free display list before continuing
                 continue;
             }
        }
         else if (strcmp(action, "edit_task") == 0) {
             cJSON *index_item = cJSON_GetObjectItem(params, "index");
             if (cJSON_IsNumber(index_item)) {
                 int index_1based = (int)index_item->valuedouble;
                 if (index_1based >= 1 && (size_t)index_1based <= disp_count) {
                     size_t index_0based = index_1based - 1;
                     Task *target_task = disp[index_0based]; // Get task from the *displayed* list
                     
                     // Update name if provided
                     cJSON *name_item = cJSON_GetObjectItem(params, "name");
                     if (cJSON_IsString(name_item) && name_item->valuestring[0] != '\0') {
                         free(target_task->name);
                         target_task->name = strdup(name_item->valuestring);
                     }
                     
                     // Update due date if provided (check both "due" and "due_date_iso" for compatibility)
                     cJSON *due_item = cJSON_GetObjectItem(params, "due_date_iso");
                     if (!due_item) {
                         due_item = cJSON_GetObjectItem(params, "due"); // Try alternative parameter name
                     }
                     
                     if (due_item) {
                         if (cJSON_IsString(due_item) && due_item->valuestring[0] != '\0') {
                             target_task->due = utils_parse_date(due_item->valuestring);
                         } else if (cJSON_IsNull(due_item)) {
                             target_task->due = 0; // Clear due date
                         }
                     }
                     
                     // Update tags if provided
                     cJSON *tags_item = cJSON_GetObjectItem(params, "tags");
                     if (tags_item && cJSON_IsArray(tags_item)) {
                         // Free existing tags
                         for (size_t i = 0; i < target_task->tag_count; ++i) {
                             free(target_task->tags[i]);
                         }
                         
                         // Count new tags
                         size_t new_tag_count = cJSON_GetArraySize(tags_item);
                         
                         // Allocate and copy new tags
                         if (new_tag_count > 0) {
                             target_task->tags = realloc(target_task->tags, new_tag_count * sizeof(char*));
                             target_task->tag_count = new_tag_count;
                             
                             for (size_t i = 0; i < new_tag_count; ++i) {
                                 cJSON *tag_item = cJSON_GetArrayItem(tags_item, i);
                                 if (cJSON_IsString(tag_item)) {
                                     target_task->tags[i] = strdup(tag_item->valuestring);
                                 } else {
                                     target_task->tags[i] = strdup(""); // Empty string for non-string items
                                 }
                             }
                         } else {
                             free(target_task->tags);
                             target_task->tags = NULL;
                             target_task->tag_count = 0;
                         }
                     }
                     
                     // Update priority if provided
                     cJSON *priority_item = cJSON_GetObjectItem(params, "priority");
                     if (priority_item && cJSON_IsString(priority_item)) {
                         if (strcasecmp(priority_item->valuestring, "high") == 0) {
                             target_task->priority = PRIORITY_HIGH;
                         } else if (strcasecmp(priority_item->valuestring, "medium") == 0) {
                             target_task->priority = PRIORITY_MEDIUM;
                         } else {
                             target_task->priority = PRIORITY_LOW;
                         }
                     }

                     // Update status if provided
                     cJSON *status_item = cJSON_GetObjectItem(params, "status");
                     if (status_item && cJSON_IsString(status_item)) {
                         if (strcasecmp(status_item->valuestring, "done") == 0) {
                             target_task->status = STATUS_DONE;
                         } else {
                             target_task->status = STATUS_PENDING;
                         }
                     }
                     
                     utils_show_message("Task updated successfully.", LINES - 2, 2);
                 } else {
                     // Provide more detailed error message with available task indices
                     char index_error[MAX_ERR_LEN];
                     snprintf(index_error, MAX_ERR_LEN, 
                              "Invalid index %d provided. Available task indices are 1-%zu. Please check the task list and use a valid index.", 
                              index_1based, disp_count);
                     snprintf(last_error, MAX_ERR_LEN, "%s", index_error);
                     free(disp); // Free display list before continuing
                     continue;
                 }
             } else {
                 snprintf(last_error, MAX_ERR_LEN, "Missing/invalid 'index' param for edit_task.");
                 free(disp); // Free display list before continuing
                 continue;
             }
         }
         else if (strcmp(action, "selected_task") == 0) {
             // Handle actions on the currently selected task
             if (disp_count == 0) {
                 snprintf(last_error, MAX_ERR_LEN, "No tasks available to select.");
                 free(disp);
                 continue;
             }
             
             // Get the nested action and params
             cJSON *nested_action_item = cJSON_GetObjectItem(params, "action");
             cJSON *nested_params_item = cJSON_GetObjectItem(params, "params");
             
             if (!cJSON_IsString(nested_action_item) || !cJSON_IsObject(nested_params_item)) {
                 snprintf(last_error, MAX_ERR_LEN, "selected_task requires 'action' and 'params' fields.");
                 free(disp);
                 continue;
             }
             
             const char *nested_action = nested_action_item->valuestring;
             cJSON *nested_params = nested_params_item;
             
             // Create a new params object with the index of the selected task
             cJSON *modified_params = cJSON_Duplicate(nested_params, 1);
             if (!modified_params) {
                 snprintf(last_error, MAX_ERR_LEN, "Failed to create parameters for selected task action.");
                 free(disp);
                 continue;
             }
             
             // Add the index of the selected task
             cJSON_AddNumberToObject(modified_params, "index", selected + 1); // 1-based index
             
             // Create a temporary string representation for logging
             char *temp_str = cJSON_PrintUnformatted(modified_params);
             if (temp_str) {
                 free(temp_str);
             }
             
             // Handle the nested action with the modified params
             if (strcmp(nested_action, "mark_done") == 0) {
                 // Mark the selected task as done
                 Task *target_task = disp[selected];
                 target_task->status = STATUS_DONE;
                 utils_show_message("Selected task marked done.", LINES - 2, 2);
             }
             else if (strcmp(nested_action, "delete_task") == 0) {
                 // Delete the selected task
                 Task *target_task = disp[selected];
                 
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
                             free(disp);
                             cJSON_Delete(modified_params);
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
                             free(disp);
                             cJSON_Delete(modified_params);
                             continue;
                         }
                         new_tasks[0] = NULL; // Ensure null termination
                     }
                     
                     // Update task list and count
                     free(tasks);
                     tasks = new_tasks;
                     count--;
                     utils_show_message("Selected task deleted.", LINES - 2, 2);
                 } else {
                     snprintf(last_error, MAX_ERR_LEN, "Task inconsistency: Cannot find selected task in main list.");
                     free(disp);
                     cJSON_Delete(modified_params);
                     continue;
                 }
             }
             else if (strcmp(nested_action, "edit_task") == 0) {
                 // Edit the selected task
                 Task *target_task = disp[selected];
                 
                 // Update name if provided
                 cJSON *name_item = cJSON_GetObjectItem(modified_params, "name");
                 if (cJSON_IsString(name_item) && name_item->valuestring[0] != '\0') {
                     free(target_task->name);
                     target_task->name = strdup(name_item->valuestring);
                 }
                 
                 // Update due date if provided (check both "due" and "due_date_iso" for compatibility)
                 cJSON *due_item = cJSON_GetObjectItem(modified_params, "due_date_iso");
                 if (!due_item) {
                     due_item = cJSON_GetObjectItem(modified_params, "due"); // Try alternative parameter name
                 }
                 
                 if (due_item) {
                     if (cJSON_IsString(due_item) && due_item->valuestring[0] != '\0') {
                         target_task->due = utils_parse_date(due_item->valuestring);
                     } else if (cJSON_IsNull(due_item)) {
                         target_task->due = 0; // Clear due date
                     }
                 }
                 
                 // Update tags if provided
                 cJSON *tags_item = cJSON_GetObjectItem(modified_params, "tags");
                 if (tags_item && cJSON_IsArray(tags_item)) {
                     // Free existing tags
                     for (size_t i = 0; i < target_task->tag_count; ++i) {
                         free(target_task->tags[i]);
                     }
                     
                     // Count new tags
                     size_t new_tag_count = cJSON_GetArraySize(tags_item);
                     
                     // Allocate and copy new tags
                     if (new_tag_count > 0) {
                         target_task->tags = realloc(target_task->tags, new_tag_count * sizeof(char*));
                         target_task->tag_count = new_tag_count;
                         
                         for (size_t i = 0; i < new_tag_count; ++i) {
                             cJSON *tag_item = cJSON_GetArrayItem(tags_item, i);
                             if (cJSON_IsString(tag_item)) {
                                 target_task->tags[i] = strdup(tag_item->valuestring);
                             } else {
                                 target_task->tags[i] = strdup(""); // Empty string for non-string items
                             }
                         }
                     } else {
                         free(target_task->tags);
                         target_task->tags = NULL;
                         target_task->tag_count = 0;
                     }
                 }
                 
                 // Update priority if provided
                 cJSON *priority_item = cJSON_GetObjectItem(modified_params, "priority");
                 if (priority_item && cJSON_IsString(priority_item)) {
                     if (strcasecmp(priority_item->valuestring, "high") == 0) {
                         target_task->priority = PRIORITY_HIGH;
                     } else if (strcasecmp(priority_item->valuestring, "medium") == 0) {
                         target_task->priority = PRIORITY_MEDIUM;
                     } else {
                         target_task->priority = PRIORITY_LOW;
                     }
                 }

                 // Update status if provided
                 cJSON *status_item = cJSON_GetObjectItem(modified_params, "status");
                 if (status_item && cJSON_IsString(status_item)) {
                     if (strcasecmp(status_item->valuestring, "done") == 0) {
                         target_task->status = STATUS_DONE;
                     } else {
                         target_task->status = STATUS_PENDING;
                     }
                 }
                 
                 utils_show_message("Selected task updated successfully.", LINES - 2, 2);
             }
             else {
                 snprintf(last_error, MAX_ERR_LEN, "Unknown nested action '%s' for selected_task.", nested_action);
                 free(disp);
                 cJSON_Delete(modified_params);
                 continue;
             }
             
             // Clean up
             cJSON_Delete(modified_params);
         }
         else if (strcmp(action, "search_tasks") == 0) {
             cJSON *term_item = cJSON_GetObjectItem(params, "term");
             if (cJSON_IsString(term_item)) {
                 strncpy(search_term, term_item->valuestring, sizeof(search_term) - 1);
                 search_term[sizeof(search_term) - 1] = '\0'; // Ensure null termination
                 utils_show_message("Search applied.", LINES - 2, 2);
             } else if (cJSON_IsNull(term_item)) {
                  search_term[0] = '\0'; // Clear search
                  utils_show_message("Search cleared.", LINES - 2, 2);
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
             utils_show_message("Displaying all tasks.", LINES - 2, 2);
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
                     utils_show_message("Tasks sorted.", LINES - 2, 2);
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
         else if (strcmp(action, "exit") == 0) {
             // Handle explicit exit action from the LLM
             utils_show_message("Exiting AI chat mode...", LINES - 2, 1);
             cJSON_Delete(root);
             free(disp);
             break; // Exit the main loop
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
