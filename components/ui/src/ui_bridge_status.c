#include "ui_bridge_status.h"

bool ui_bridge_status_update(ui_bridge_status_t *status, bool online, uint64_t generation)
{
    /* A generation that went offline never comes back online: a new stream gets a new generation. */
    if (generation < status->generation ||
        (generation == status->generation && online && !status->online)) {
        return false;
    }
    bool changed = status->online != online;
    status->generation = generation;
    status->online = online;
    return changed;
}
