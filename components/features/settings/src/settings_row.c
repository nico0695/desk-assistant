#include "settings_row.h"

static const settings_row_effect_t NO_EFFECT = {0};

static bool applies_while_dragging(const settings_row_t *row)
{
    return row->id == SETTINGS_BRIGHTNESS;
}

static settings_row_effect_t confirm(settings_row_t *row, uint8_t value)
{
    row->confirmed = value;
    row->shown = value;
    row->save_sequence++;
    return (settings_row_effect_t){.save = true, .value = value, .save_sequence = row->save_sequence};
}

void settings_row_init(settings_row_t *row, settings_id_t id, uint8_t value, bool load_failed)
{
    *row = (settings_row_t){
        .id = id, .confirmed = value, .shown = value, .applied = value, .load_failed = load_failed};
}

settings_row_effect_t settings_row_drag(settings_row_t *row, uint8_t value)
{
    if (!settings_value_valid(row->id, value) || (row->dragging && value == row->shown)) {
        return NO_EFFECT;
    }
    row->dragging = true;
    row->shown = value;
    if (!applies_while_dragging(row)) {
        return NO_EFFECT;
    }
    if (value == row->applied) {
        /* The panel already shows this value; an earlier failure is no longer current. */
        row->apply_failed = false;
        return NO_EFFECT;
    }
    return (settings_row_effect_t){.apply = true, .value = value};
}

settings_row_effect_t settings_row_release(settings_row_t *row)
{
    if (!row->dragging) {
        return NO_EFFECT;
    }
    row->dragging = false;
    const uint8_t value = applies_while_dragging(row) ? row->applied : row->shown;
    if (value == row->confirmed) {
        row->shown = value;
        return NO_EFFECT;
    }
    if (!applies_while_dragging(row)) {
        row->applied = value;
    }
    return confirm(row, value);
}

settings_row_effect_t settings_row_cancel(settings_row_t *row)
{
    if (!row->dragging) {
        return NO_EFFECT;
    }
    row->dragging = false;
    row->shown = row->confirmed;
    if (!applies_while_dragging(row)) {
        return NO_EFFECT;
    }
    if (row->applied == row->confirmed) {
        row->apply_failed = false;
        return NO_EFFECT;
    }
    return (settings_row_effect_t){.apply = true, .value = row->confirmed};
}

settings_row_effect_t settings_row_select(settings_row_t *row, uint8_t value)
{
    if (!settings_value_valid(row->id, value) || value == row->confirmed) {
        return NO_EFFECT;
    }
    settings_row_effect_t effect = confirm(row, value);
    effect.apply = true;
    return effect;
}

void settings_row_apply_result(settings_row_t *row, uint8_t value, bool ok)
{
    row->apply_failed = !ok;
    if (ok) {
        row->applied = value;
    }
}

void settings_row_save_result(settings_row_t *row, uint32_t save_sequence, bool ok)
{
    if (save_sequence != row->save_sequence) {
        return;
    }
    row->save_failed = !ok;
    if (ok) {
        row->load_failed = false;
    }
}

settings_row_error_t settings_row_error(const settings_row_t *row)
{
    if (row->apply_failed) {
        return SETTINGS_ROW_ERROR_APPLY;
    }
    if (row->save_failed) {
        return SETTINGS_ROW_ERROR_SAVE;
    }
    return row->load_failed ? SETTINGS_ROW_ERROR_LOAD : SETTINGS_ROW_ERROR_NONE;
}
