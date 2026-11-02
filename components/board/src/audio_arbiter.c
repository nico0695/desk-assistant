#include "audio_arbiter.h"

enum { AUDIO_OWNED = 1U, AUDIO_CAPTURE_INTENT = 2U };

static audio_arbiter_t shared_audio;

audio_arbiter_t *board_audio_arbiter(void) { return &shared_audio; }

void audio_arbiter_capture_intent(audio_arbiter_t *arbiter, bool active)
{
    if (active) atomic_fetch_or(&arbiter->bits, AUDIO_CAPTURE_INTENT);
    else atomic_fetch_and(&arbiter->bits, ~AUDIO_CAPTURE_INTENT);
}

bool audio_arbiter_try_capture(audio_arbiter_t *arbiter)
{
    unsigned expected = atomic_load(&arbiter->bits);
    while ((expected & AUDIO_OWNED) == 0U) {
        if (atomic_compare_exchange_weak(&arbiter->bits, &expected, expected | AUDIO_OWNED)) return true;
    }
    return false;
}

bool audio_arbiter_try_tone(audio_arbiter_t *arbiter)
{
    unsigned expected = 0U;
    return atomic_compare_exchange_strong(&arbiter->bits, &expected, AUDIO_OWNED);
}

void audio_arbiter_release(audio_arbiter_t *arbiter)
{
    atomic_fetch_and(&arbiter->bits, ~AUDIO_OWNED);
}
