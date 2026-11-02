#pragma once

#include <stdbool.h>
#include <stdatomic.h>

typedef struct { atomic_uint bits; } audio_arbiter_t;

audio_arbiter_t *board_audio_arbiter(void);
void audio_arbiter_capture_intent(audio_arbiter_t *arbiter, bool active);
bool audio_arbiter_try_capture(audio_arbiter_t *arbiter);
bool audio_arbiter_try_tone(audio_arbiter_t *arbiter);
void audio_arbiter_release(audio_arbiter_t *arbiter);
