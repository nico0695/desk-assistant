#include "touch_gate.h"

void touch_gate_init(touch_gate_t *gate)
{
    *gate = (touch_gate_t){.enabled = true};
}

void touch_gate_set_enabled(touch_gate_t *gate, bool enabled)
{
    gate->enabled = enabled;
    if (enabled) {
        return;
    }
    /* A hold in progress when the panel turns off must not wake it right away; a new gesture can. */
    gate->activity = false;
    if (gate->raw_pressed) {
        gate->blocked = true;
        gate->suppress_activity_until_release = true;
    }
}

bool touch_gate_sample(touch_gate_t *gate, bool valid, bool pressed)
{
    if (!valid) {
        /* An uncertain release cancels the press instead of clicking; the hold stays hidden until a real release. */
        if (gate->raw_pressed) {
            gate->blocked = true;
        }
        return false;
    }
    gate->raw_pressed = pressed;
    if (!pressed) {
        gate->blocked = false;
        gate->suppress_activity_until_release = false;
        return false;
    }
    if (!gate->suppress_activity_until_release) {
        gate->activity = true;
    }
    if (!gate->enabled) {
        gate->blocked = true;
    }
    return !gate->blocked;
}

bool touch_gate_held(const touch_gate_t *gate)
{
    return gate->raw_pressed;
}

bool touch_gate_blocked(const touch_gate_t *gate)
{
    return gate->blocked;
}

bool touch_gate_take_activity(touch_gate_t *gate)
{
    const bool activity = gate->activity;
    gate->activity = false;
    return activity;
}
