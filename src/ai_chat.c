#include "ai_chat.h"
#include "llm_api.h"
#include "ui.h"         // For ncurses UI functions
#include "storage.h"    // For loading/saving tasks
#include "task.h"       // For task manipulation
#include "utils.h"      // For common utilities
#include "task_manager.h" // For centralized task management
#include "ai_chat_actions.h" // For action handlers
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
             "- To reference the currently selected task (marked with an arrow \u2192 and 'SELECTED' in the list), use the 'selected_task' action.\n"
             "- For project management, you can create or delete projects by name, and assign tasks to a specific project.\n"
             "- Projects can only be deleted if they have no tasks.\n"
             "\n"
             "Supported Actions & Params:\n"
             " add_task: { \"name\": string, \"due\": \"YYYY-MM-DD\" | null, \"tags\": [string], \"priority\": \"low\"|\"medium\"|\"high\", \"project\": string }\n"
             " mark_done: { \"index\": number }\n"
             " delete_task: { \"index\": number }\n"
             " edit_task: { \"index\": number, \"name\": string?, \"due\": \"YYYY-MM-DD\" | null?, \"tags\": [string]?, \"priority\": string?, \"status\": string? }\n"
             " selected_task: { \"action\": \"mark_done\" | \"delete_task\" | \"edit_task\", \"params\": {...} } (Apply an action to the currently selected task)\n"
             " add_project: { \"name\": string }\n"
             " delete_project: { \"name\": string } (Only allowed if the project has no tasks)\n"
             " search_tasks: { \"term\": string | null } (null term clears search)\n"
             " filter_by_date: { \"range\": \"today\"|\"tomorrow\"|\"this_week\"|\"next_week\"|\"overdue\" } (Filter tasks by date range)\n"
             " filter_by_priority: { \"level\": \"high\"|\"medium\"|\"low\" } (Filter tasks by priority)\n"
             " filter_by_status: { \"status\": \"done\"|\"pending\" } (Filter tasks by completion status)\n"
             " filter_combined: { \"filters\": [ {\"type\": \"date\"|\"priority\"|\"status\", \"value\": string}, ... ] } (Apply multiple filters)\n"
             " sort_tasks: { \"by\": \"name\"|\"due\"|\"creation\" }\n"
             " list_tasks: {} (Use this if the user asks to see tasks, effectively clears search)\n"
             " exit: {} (Use this to exit the AI chat mode)\n"
             "\n"
             "Context:\n"
             "%s" // Task list context added here
             "\n"
             "Example (Add): User: \"add buy milk tomorrow high prio\" -> {\"action\":\"add_task\",\"params\":{\"name\":\"buy milk\",\"due\":\"YYYY-MM-DD\",\"tags\":[],\"priority\":\"high\"}}\n"
             "Example (Add Project): User: \"create new project Health\" -> {\"action\":\"add_project\",\"params\":{\"name\":\"Health\"}}\n"
             "Example (Delete Project): User: \"delete project Health\" -> {\"action\":\"delete_project\",\"params\":{\"name\":\"Health\"}}\n"
             "Example (Task in Project): User: \"create a new task Do workout at Gym in project Health\" -> {\"action\":\"add_task\",\"params\":{\"name\":\"Do workout at Gym\",\"project\":\"Health\"}}\n"
             "Example (Mark): User: \"mark item 2 done\" -> {\"action\":\"mark_done\",\"params\":{\"index\":2}}\n"
             "Example (Edit): User: \"change due date of task 3 to next Friday\" -> {\"action\":\"edit_task\",\"params\":{\"index\":3,\"due\":\"YYYY-MM-DD\"}}\n"
             "Example (Selected): User: \"update the due date of the selected task to tomorrow\" -> {\"action\":\"selected_task\",\"params\":{\"action\":\"edit_task\",\"params\":{\"due\":\"YYYY-MM-DD\"}}}\n"
             "Example (Search): User: \"find tasks related to 'project x'\" -> {\"action\":\"search_tasks\",\"params\":{\"term\":\"project x\"}}\n"
             "Example (Date Filter): User: \"What tasks are due this week?\" -> {\"action\":\"filter_by_date\",\"params\":{\"range\":\"this_week\"}}\n"
             "Example (Priority Filter): User: \"Show me all high priority tasks\" -> {\"action\":\"filter_by_priority\",\"params\":{\"level\":\"high\"}}\n"
             "Example (Status Filter): User: \"Show me completed tasks\" -> {\"action\":\"filter_by_status\",\"params\":{\"status\":\"done\"}}\n"
             "Example (Combined Filter): User: \"Show me low priority tasks due next week\" -> {\"action\":\"filter_combined\",\"params\":{\"filters\":[{\"type\":\"priority\",\"value\":\"low\"},{\"type\":\"date\",\"value\":\"next_week\"}]}}\n"
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
            "Valid actions: 'add_task' (name, due_date_iso?, tags?, priority?, project?), 'mark_done' (index), 'delete_task' (index), 'edit_task' (index, name?, due_date_iso?, tags?, priority?), 'selected_task' (action, params), 'search_tasks' (term), 'sort_tasks' (field: name|due|prio), 'list_tasks' (no params), 'exit' (no params). " \
            "For dates, use ISO format YYYY-MM-DD.");

end_prompt:
    return;
}

// Generate an AI-powered suggestion for a task
static char* generate_ai_suggestion(Task *task) {
    if (!task) return NULL;
    
    // Allocate memory for the suggestion
    char *suggestion = utils_calloc(1, 128);
    if (!suggestion) return NULL;
    
    // Format the due date if available
    char due_str[32] = "no due date";
    if (task->due > 0) {
        struct tm tm_due;
        gmtime_r(&task->due, &tm_due);
        strftime(due_str, sizeof(due_str), "%Y-%m-%d", &tm_due);
    }
    
    // Format the priority
    const char *priority_str = "low";
    if (task->priority == PRIORITY_HIGH) {
        priority_str = "high";
    } else if (task->priority == PRIORITY_MEDIUM) {
        priority_str = "medium";
    }
    
    // Create a system prompt for the suggestion
    char system_prompt[512];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are a helpful task assistant that provides brief, actionable suggestions. "
        "Respond with ONLY a single, concise suggestion (max 50 chars) for how to approach this task. "
        "Do not include any explanations, prefixes, or formatting. Just the suggestion text.");
    
    // Create a user prompt with task details
    char user_prompt[512];
    snprintf(user_prompt, sizeof(user_prompt),
        "Task: %s\nPriority: %s\nDue date: %s\nStatus: %s\n\n"
        "Give me a brief, actionable suggestion for how to approach this task.",
        task->name,
        priority_str,
        due_str,
        task->status == STATUS_DONE ? "completed" : "pending");
    
    // Call the LLM API with a smaller, faster model for suggestions
    char *raw_response = NULL;
    if (llm_chat(system_prompt, user_prompt, &raw_response, 0, "gpt-4.1-nano") != 0 || !raw_response) {
        // If API call fails, provide a fallback suggestion
        if (task->priority == PRIORITY_HIGH) {
            strcpy(suggestion, "Break into smaller steps");
        } else {
            strcpy(suggestion, "Consider prioritizing this task");
        }
        return suggestion;
    }
    
    // Parse the JSON response
    cJSON *json = cJSON_Parse(raw_response);
    if (!json) {
        // JSON parsing failed, use the raw response but truncate if needed
        size_t len = strlen(raw_response);
        if (len > 127) len = 127;
        strncpy(suggestion, raw_response, len);
        suggestion[len] = '\0';
        free(raw_response);
        return suggestion;
    }
    
    // Extract the content from the JSON response
    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
        if (first_choice) {
            cJSON *message = cJSON_GetObjectItem(first_choice, "message");
            if (message) {
                cJSON *content = cJSON_GetObjectItem(message, "content");
                if (content && cJSON_IsString(content) && content->valuestring) {
                    // Copy the content to our suggestion buffer
                    strncpy(suggestion, content->valuestring, 127);
                    suggestion[127] = '\0';
                }
            }
        }
    }
    
    // If we couldn't extract the content, provide a fallback
    if (suggestion[0] == '\0') {
        if (task->priority == PRIORITY_HIGH) {
            strcpy(suggestion, "Break into smaller steps");
        } else {
            strcpy(suggestion, "Consider prioritizing this task");
        }
    }
    
    // Clean up
    cJSON_Delete(json);
    free(raw_response);
    
    return suggestion;
}

// --- Main AI Chat REPL Function ---

int ai_chat_repl(void) {
    // Initialize task manager
    if (task_manager_init() != 0) {
        fprintf(stderr, "Failed to initialize task manager.\n");
        return 1;
    }

    // Load tasks
    size_t count = 0;
    Task **tasks = task_manager_load_tasks(&count);
    if (!tasks) {
        // Allow starting with an empty list if file doesn't exist/is empty
        tasks = utils_calloc(1, sizeof(Task*)); // Need space for NULL terminator
        if (!tasks) {
            fprintf(stderr, "Failed to allocate initial task list.\n");
            return 1;
        }
        tasks[0] = NULL;
        count = 0;
        // Optional: Inform user if storage was newly created or empty
    }

    // Load projects
    task_manager_load_projects();
    char **projects = NULL;
    size_t project_count = task_manager_get_projects(&projects);
    size_t selected_project_idx = 0;
    const char *current_project = projects[selected_project_idx];

    // Initialize UI
    if (ui_init() != 0) {
        fprintf(stderr, "Failed to initialize UI.\n");
        task_manager_cleanup(tasks, count);
        free(projects);
        return 1;
    }

    size_t selected = 0; // Keep track of selection for potential future use (e.g., showing context)
    char search_term[64] = ""; // For potential future search integration
    char last_error[MAX_ERR_LEN] = ""; // To display errors

    Task **disp = malloc((count + 1) * sizeof(Task*));
    if (!disp) {
        ui_teardown();
        task_manager_cleanup(tasks, count);
        free(projects);
        fprintf(stderr, "Failed to allocate memory for display list.\n");
        return 1;
    }

    while (1) {
        // --- Project sidebar logic ---
        // Use central project list
        if (projects) {
            free(projects);
            projects = NULL;
        }
        project_count = task_manager_get_projects(&projects);
        if (selected_project_idx >= project_count && project_count > 0) selected_project_idx = project_count - 1;
        current_project = projects[selected_project_idx];

        // Build display list for current project
        size_t disp_count = 0;
        for (size_t i = 0; i < count; ++i) {
            if ((search_term[0] == '\0' || task_matches_search(tasks[i], search_term)) &&
                strcmp(tasks[i]->project, current_project) == 0) {
                disp[disp_count++] = tasks[i];
            }
        }
        disp[disp_count] = NULL;
        if (selected >= disp_count && disp_count > 0) selected = disp_count - 1;
        if (disp_count == 0) selected = 0;

        // Draw UI
        clear();
        ui_draw_header(search_term[0] ? search_term : "AI Chat Mode");
        ui_draw_projects(projects, project_count, selected_project_idx);
        ui_draw_tasks(disp, disp_count, selected);

        // Suggestion under task list
        int suggestion_y = 3 + (int)disp_count;
        if (disp_count > 0 && selected < disp_count) {
            Task *selected_task = disp[selected];
            if (selected_task->priority == PRIORITY_HIGH || 
                (selected_task->due > 0 && selected_task->due < time(NULL))) {
                char *ai_suggestion = generate_ai_suggestion(selected_task);
                if (ai_suggestion && ai_suggestion[0]) {
                    ui_draw_suggestion(suggestion_y, ai_suggestion);
                    free(ai_suggestion);
                }
            }
        }

        ui_draw_ai_chat_footer();
        if (last_error[0] != '\0') {
             int err_y = LINES - 2;
             attron(A_BOLD | COLOR_PAIR(CP_OVERDUE));
             mvprintw(err_y, 1, "Error: %s", last_error);
             attroff(A_BOLD | COLOR_PAIR(CP_OVERDUE));
             last_error[0] = '\0';
        }
        refresh();

        // Get user input - handle navigation keys first
        int ch = ui_get_input();
        if (ch == 'q') {
            free(disp); 
            break;
        }
        
        // Handle navigation keys
        switch (ch) {
            case KEY_LEFT:
            case 'h':
                if (selected_project_idx > 0) selected_project_idx--;
                current_project = projects[selected_project_idx];
                selected = 0; // Reset task selection when changing projects
                continue;
            case KEY_RIGHT:
            case 'l':
                if (selected_project_idx + 1 < project_count) selected_project_idx++;
                current_project = projects[selected_project_idx];
                selected = 0; // Reset task selection when changing projects
                continue;
            case '+': { // Add new project
                char proj_name[64];
                prompt_input("New project name:", proj_name, sizeof(proj_name));
                if (proj_name[0] != '\0') {
                    if (task_manager_add_project(proj_name) == 0) {
                        task_manager_save_projects();
                        free(projects);
                        project_count = task_manager_get_projects(&projects);
                        selected_project_idx = project_count - 1;
                        current_project = projects[selected_project_idx];
                    } else {
                        utils_show_message("Failed to add project", LINES-2, 2);
                    }
                }
                continue;
            }
            case '-': { // Delete project
                if (project_count <= 1) { continue; }
                const char *to_delete = projects[selected_project_idx];
                if (strcmp(to_delete, "default") == 0) { continue; }
                int del_result = task_manager_delete_project(to_delete, tasks, count);
                if (del_result == 0) {
                    task_manager_save_projects();
                    free(projects);
                    project_count = task_manager_get_projects(&projects);
                    if (selected_project_idx >= project_count) selected_project_idx = project_count - 1;
                    current_project = projects[selected_project_idx];
                } else {
                    utils_show_message("Only projects without tasks can be deleted", LINES-2, 2);
                }
                continue;
            }
            case KEY_DOWN:
            case 'j':
                if (selected + 1 < disp_count) selected++;
                continue;
            case KEY_UP:
            case 'k':
                if (selected > 0) selected--;
                continue;
            case 'm': {
                if (disp_count == 0) { continue; }
                Task *t = disp[selected];
                t->status = (t->status == STATUS_DONE) ? STATUS_PENDING : STATUS_DONE;
                utils_show_message(t->status == STATUS_DONE ? "Task marked as done." : "Task marked as pending.", LINES-2, 2);
                continue;
            }
                
            case 10: // Enter key
                // Only proceed to AI chat when Enter is pressed
                break;
                
            default:
                continue;
        }
        
        // Get command input when Enter is pressed
        char user_input[MAX_MSG_LEN];
        prompt_input("Enter command:", user_input, sizeof(user_input));

        if (strcmp(user_input, "exit") == 0 || strlen(user_input) == 0) {
            free(disp); 
            break;
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
                         (i == selected) ? "\u2192 " : "  ", // Add arrow indicator for selected task
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
        int llm_status = llm_chat(sys_prompt, user_input, &llm_response_json, 0, NULL); // temperature=0 for deterministic JSON

        if (llm_status != 0 || !llm_response_json || llm_response_json[0] == '\0') {
            snprintf(last_error, MAX_ERR_LEN, "AI interaction failed (status: %d)", llm_status);
            if (llm_response_json) free(llm_response_json);
            continue; // Go back to draw the error
        }

        // 3. Parse JSON Response - first extract content from OpenAI response
        cJSON *api_response = cJSON_Parse(llm_response_json);
        if (!api_response) {
            snprintf(last_error, MAX_ERR_LEN, "Failed to parse API response JSON");
            free(llm_response_json);
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
            continue;
        }
        
        // Now parse the actual content as JSON
        cJSON *root = cJSON_Parse(content);
        free(content); // Free the extracted content string

        if (!root) {
            snprintf(last_error, MAX_ERR_LEN, "AI response was not valid JSON");
            continue;
        }

        cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "action");
        cJSON *params_item = cJSON_GetObjectItemCaseSensitive(root, "params");

        if (!cJSON_IsString(action_item) || !cJSON_IsObject(params_item)) {
            utils_show_message("Request not understood. No action taken.", LINES-2, 2);
            cJSON_Delete(root);
            continue;
        }

        const char *action = action_item->valuestring;
        cJSON *params = params_item;

        // 4. Execute Action
        ActionResult result = ACTION_ERROR;

        if (strcmp(action, "add_task") == 0) {
            result = handle_add_task(params, &tasks, &count, current_project, last_error);
        } else if (strcmp(action, "delete_task") == 0) {
            result = handle_delete_task(params, &tasks, &count, disp, disp_count, &selected, last_error);
        } else if (strcmp(action, "edit_task") == 0) {
            result = handle_edit_task(params, disp, disp_count, last_error);
        } else if (strcmp(action, "mark_done") == 0) {
            result = handle_mark_done(params, disp, disp_count, last_error);
        } else if (strcmp(action, "edit_task_status") == 0) {
            result = handle_edit_task_status(params, disp, disp_count, last_error);
        } else if (strcmp(action, "selected_task") == 0) {
            result = handle_selected_task(params, &tasks, &count, disp, disp_count, &selected, &selected, last_error);
        } else if (strcmp(action, "sort_tasks") == 0) {
            result = handle_sort_tasks(params, tasks, count, last_error);
        } else if (strcmp(action, "filter_by_date") == 0) {
            result = handle_filter_by_date(params, search_term, sizeof(search_term), last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection
        } else if (strcmp(action, "filter_by_priority") == 0) {
            result = handle_filter_by_priority(params, search_term, sizeof(search_term), last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection
        } else if (strcmp(action, "filter_by_status") == 0) {
            result = handle_filter_by_status(params, search_term, sizeof(search_term), last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection
        } else if (strcmp(action, "filter_combined") == 0) {
            result = handle_filter_combined(params, search_term, sizeof(search_term), last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection
        } else if (strcmp(action, "search_tasks") == 0) {
            result = handle_search_tasks(params, search_term, sizeof(search_term), last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection on search change
        } else if (strcmp(action, "list_tasks") == 0) {
            result = handle_list_tasks(search_term, last_error);
            if (result == ACTION_SUCCESS) selected = 0; // Reset selection
        } else if (strcmp(action, "add_project") == 0) {
            result = handle_add_project(params, &projects, &project_count, &selected_project_idx, 
                                        &current_project, tasks, count, last_error);
        } else if (strcmp(action, "delete_project") == 0) {
            result = handle_delete_project(params, &projects, &project_count, &selected_project_idx, 
                                           &current_project, tasks, count, last_error);
        } else if (strcmp(action, "exit") == 0) {
            result = handle_exit(params, last_error);
            if (result == ACTION_EXIT) {
                cJSON_Delete(root);
                free(disp);
                break; // Exit the main loop
            }
        } else {
            snprintf(last_error, MAX_ERR_LEN, "Unknown action '%s' received from AI.", action);
            cJSON_Delete(root);
            continue;
        }

        cJSON_Delete(root);
    }

    // Cleanup
    ui_teardown();

    // Save tasks before exiting
    if (task_manager_save_tasks(tasks, count) != 0) {
        // Don't print to stderr as ncurses is torn down. Maybe log?
    }

    task_manager_save_projects();
    task_manager_cleanup(tasks, count);
    free(projects);

    printf("Exiting AI chat mode.\n"); // Print after ncurses is done
    return 0;
}
