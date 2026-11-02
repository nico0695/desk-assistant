/* RAM-only saturation policy adapted from the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "notification_tones.h"

void notification_tones_add(notification_tones_t *tones)
{
    if (tones->pending < NOTIFICATION_TONES_MAX_PENDING) ++tones->pending;
}

bool notification_tones_take(notification_tones_t *tones)
{
    if (tones->pending == 0U) return false;
    --tones->pending;
    return true;
}
