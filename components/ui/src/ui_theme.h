#pragma once

#include "board_display_geometry.h"
#include "lvgl.h"

#define UI_COLOR_BACKGROUND 0x0a0f14
#define UI_COLOR_SURFACE 0x151c22
#define UI_COLOR_SURFACE_RAISED 0x202b33
#define UI_COLOR_CONTROL 0x30424d
#define UI_COLOR_CONTROL_PRESSED 0x526d7b
#define UI_COLOR_PRIMARY 0x526d7b
#define UI_COLOR_PRIMARY_PRESSED 0x6b8796
#define UI_COLOR_TEXT 0xedf1f2
#define UI_COLOR_TEXT_SECONDARY 0xbdc8cd
#define UI_COLOR_RECORD 0xf87171
#define UI_COLOR_WARNING 0xfbbf24
#define UI_COLOR_WARNING_TEXT 0xfde68a
#define UI_COLOR_SUCCESS 0x86efac
#define UI_COLOR_ERROR 0xfca5a5

#define UI_SCREEN_MARGIN_PX 24
#define UI_CONTENT_WIDTH_PX (BOARD_DISPLAY_WIDTH_PX - 2 * UI_SCREEN_MARGIN_PX)
#define UI_GAP_PX 16
#define UI_ICON_TEXT_GAP_PX 12
#define UI_CORNER_RADIUS_PX 16
#define UI_BUTTON_HEIGHT_PX 72
#define UI_PRIMARY_BUTTON_HEIGHT_PX 120
#define UI_TILE_HEIGHT_PX 120
#define UI_STATUS_BAR_HEIGHT_PX 48
#define UI_STATUS_BAR_SIDE_INSET_PX 40
#define UI_RECORD_DOT_SIZE_PX 24
#define UI_WIFI_SLASH_WIDTH_PX 3

typedef enum {
    UI_FONT_SECONDARY,
    UI_FONT_BODY,
    UI_FONT_TITLE,
} ui_font_role_t;

typedef struct {
    lv_style_t screen;
    lv_style_t button;
    lv_style_t button_pressed;
    lv_style_t button_primary;
    lv_style_t button_primary_pressed;
    lv_style_t text_secondary;
} ui_styles_t;

/* Call once from the UI owner after lv_init(). */
void ui_theme_init(void);

const lv_font_t *ui_theme_font(ui_font_role_t role);
ui_styles_t *ui_theme_styles(void);
