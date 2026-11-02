#pragma once

#include <stdbool.h>

#include "lvgl.h"

/* True when font, with its fallbacks, has a real glyph for every character of text; line breaks and tabs count. */
bool ui_text_support_covers(const lv_font_t *font, const char *text);
