#pragma once
#include <stddef.h>
#include <stdint.h>

void audio_path_init(void);
void audio_path_play_beep(uint16_t hz, uint16_t ms);
void audio_path_speak_text_stub(const char *text);
int audio_path_read_mic(int16_t *dst, size_t samples);
void audio_path_write_pcm(const int16_t *src, size_t samples);
