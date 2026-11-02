#include "ui_navigation.h"

#include <stdint.h>

#include "ui_home.h"
#include "ui_modules_control.h"
#include "ui_recording_control.h"
#include "ui_section.h"
#include "ui_settings_control.h"
#include "ui_wifi_control.h"

static void replace_active_screen(lv_obj_t *screen)
{
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

static void show_home(void *unused);
static void show_settings(void *unused);
static void show_wifi(void *unused);

static void request_home(lv_event_t *event)
{
    (void)event;
    /* Deferred: the screen that sent the event is deleted on replacement. */
    lv_async_call(show_home, NULL);
}

static void request_settings(lv_event_t *event)
{
    (void)event;
    lv_async_call(show_settings, NULL);
}

static void request_wifi(lv_event_t *event)
{
    (void)event;
    lv_async_call(show_wifi, NULL);
}

static void show_settings(void *unused)
{
    (void)unused;
    replace_active_screen(ui_settings_control_create_screen(request_home, request_wifi));
}

static void show_wifi(void *unused)
{
    (void)unused;
    replace_active_screen(ui_wifi_control_create_screen(request_settings));
}

static void leave_module(void)
{
    lv_async_call(show_home, NULL);
}

static void show_section(void *section_data)
{
    const ui_section_t section = (ui_section_t)(uintptr_t)section_data;
    if (section == UI_SECTION_SETTINGS) {
        show_settings(NULL);
        return;
    }
    static const bridge_resource_kind_t KINDS[UI_SECTION_COUNT] = {
        [UI_SECTION_TASKS] = BRIDGE_RESOURCE_TASKS,
        [UI_SECTION_NOTES] = BRIDGE_RESOURCE_NOTES,
        [UI_SECTION_ALERTS] = BRIDGE_RESOURCE_ALERTS,
    };
    replace_active_screen(ui_modules_control_open(KINDS[section], leave_module));
}

static void request_section(ui_section_t section)
{
    lv_async_call(show_section, (void *)(uintptr_t)section);
}

static void leave_recording(void)
{
    lv_async_call(show_home, NULL);
}

static void show_recording(void *unused)
{
    (void)unused;
    lv_obj_t *panel = ui_recording_control_open(leave_recording);
    if (panel != NULL) {
        replace_active_screen(panel);
    }
}

static void request_recording(lv_event_t *event)
{
    (void)event;
    lv_async_call(show_recording, NULL);
}

static void show_home(void *unused)
{
    (void)unused;
    replace_active_screen(ui_home_screen_create(request_section, request_recording, ui_recording_control_entry_text()));
}

void ui_navigation_start(void)
{
    show_home(NULL);
}
