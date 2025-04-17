#ifndef AI_ASSIST_H
#define AI_ASSIST_H

#ifndef AI_SMART_ADD_DEBUG
#define AI_SMART_ADD_DEBUG 0
#endif

void ai_smart_add(const char *prompt, int debug);
#define ai_smart_add_default(prompt) ai_smart_add((prompt), AI_SMART_ADD_DEBUG)

#endif // AI_ASSIST_H
