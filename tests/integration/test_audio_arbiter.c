#include "audio_arbiter.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void capture_intent_blocks_new_tone_and_waits_for_current_tone(void)
{
    audio_arbiter_t arbiter = {0};
    TEST_ASSERT_TRUE(audio_arbiter_try_tone(&arbiter));
    audio_arbiter_capture_intent(&arbiter, true);
    TEST_ASSERT_FALSE(audio_arbiter_try_capture(&arbiter));
    audio_arbiter_release(&arbiter);
    TEST_ASSERT_FALSE(audio_arbiter_try_tone(&arbiter));
    TEST_ASSERT_TRUE(audio_arbiter_try_capture(&arbiter));
    audio_arbiter_capture_intent(&arbiter, false);
    TEST_ASSERT_FALSE(audio_arbiter_try_tone(&arbiter));
    audio_arbiter_release(&arbiter);
    TEST_ASSERT_TRUE(audio_arbiter_try_tone(&arbiter));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(capture_intent_blocks_new_tone_and_waits_for_current_tone);
    return UNITY_END();
}
