#pragma once

#include "lvgl.h"

typedef enum {
    UI_BUTTON_STANDARD,
    UI_BUTTON_PRIMARY,
    UI_BUTTON_TILE,
} ui_button_kind_t;

/* Empty screen with the theme background; the caller loads it. */
lv_obj_t *ui_screen_create(void);

/*
 * Button with an icon and a label: icon left of the text, or above it for tiles.
 * The icon is a symbol string, or NULL to leave room for a custom icon added by the caller.
 * Width and height are set by the caller's layout; the kind sets the minimum height.
 */
lv_obj_t *ui_button_create(lv_obj_t *parent, ui_button_kind_t kind, const char *symbol, const char *text);
