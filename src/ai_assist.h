#ifndef AI_ASSIST_H
#define AI_ASSIST_H

void ai_smart_add(const char *prompt, int debug);
#define ai_smart_add_default(prompt) ai_smart_add((prompt), 0)

#endif // AI_ASSIST_H
