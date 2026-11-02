#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NOTIFICATION_TONES_MAX_PENDING 8U

typedef struct { uint8_t pending; } notification_tones_t;

void notification_tones_add(notification_tones_t *tones);
bool notification_tones_take(notification_tones_t *tones);
