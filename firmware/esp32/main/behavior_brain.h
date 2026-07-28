#pragma once

void behavior_brain_init(void);
void behavior_brain_task_start(void);
void behavior_brain_on_goal(const char *goal);
void behavior_brain_on_goal_ttl(const char *goal, uint32_t ttl_ms);
