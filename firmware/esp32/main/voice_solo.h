#pragma once

typedef enum {
    VOICE_CMD_NONE = 0,
    VOICE_CMD_COME,
    VOICE_CMD_STOP,
    VOICE_CMD_SLEEP,
    VOICE_CMD_PATROL,
    VOICE_CMD_TEMP,
    VOICE_CMD_BATTERY,
    VOICE_CMD_SPIN,
} voice_cmd_t;

void voice_solo_init(void);
void voice_solo_task_start(void);
voice_cmd_t voice_solo_pop_cmd(void);
void voice_solo_say(const char *text);
void voice_solo_inject(voice_cmd_t c);
