#include "bridge_sse.h"

#include <stdio.h>
#include <string.h>

#include "unity.h"

static const char ID[] = "12345678-1234-abcd-9876-123456789abc";
static const char OTHER_ID[] = "12345678-1234-abcd-9876-123456789abd";
static const char ASSISTANT_FRAME[] =
    "event: assistant_result\ndata: {\"request_id\":\"12345678-1234-abcd-9876-123456789abc\"}\n\n";

void setUp(void) {}
void tearDown(void) {}

static void open_stream(bridge_sse_t *state)
{
    bridge_sse_init(state);
    TEST_ASSERT_TRUE(bridge_sse_set_assistant(state, ID));
    bridge_sse_connected(state, 100U);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(state));
    TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(state));
}

static void feed_text(bridge_sse_t *state, const char *text, uint64_t now)
{
    TEST_ASSERT_TRUE(bridge_sse_feed(state, text, strlen(text), now));
}

static void fragmented_frames_and_crlf(void)
{
    const char *frames = ": heartbeat\r\n\r\nevent: pending_events\r\ndata: { }\r\n\r\n"
                         "event: assistant_result\ndata: {\"request_id\":"
                         "\"12345678-1234-abcd-9876-123456789abc\"}\n\n";
    const size_t length = strlen(frames);
    for (size_t split = 0; split <= length; ++split) {
        bridge_sse_t state;
        open_stream(&state);
        TEST_ASSERT_TRUE(bridge_sse_feed(&state, frames, split, 101U));
        TEST_ASSERT_TRUE(bridge_sse_feed(&state, frames + split, length - split, 102U));
        TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
        TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(&state));
        TEST_ASSERT_FALSE(bridge_sse_take_pending_events(&state));
        TEST_ASSERT_FALSE(bridge_sse_take_assistant_result(&state));
    }
    bridge_sse_t state;
    open_stream(&state);
    for (size_t i = 0; i < strlen(ASSISTANT_FRAME); ++i) {
        TEST_ASSERT_TRUE(bridge_sse_feed(&state, ASSISTANT_FRAME + i, 1U, 200U + i));
        TEST_ASSERT_EQUAL(i == strlen(ASSISTANT_FRAME) - 1U, bridge_sse_take_assistant_result(&state));
    }
}

static void heartbeat_and_reconnect_timing(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    TEST_ASSERT_TRUE(bridge_sse_retry_due(&state, 0));
    bridge_sse_connected(&state, 100U);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
    feed_text(&state, ": heartbeat\n\n", 20100U);
    TEST_ASSERT_FALSE(bridge_sse_dead(&state, 80099U));
    TEST_ASSERT_TRUE(bridge_sse_dead(&state, 80100U));
    TEST_ASSERT_FALSE(bridge_sse_dead(&state, 1U));
    TEST_ASSERT_FALSE(bridge_sse_take_pending_events(&state));
    const uint64_t delays[] = {5000U, 10000U, 30000U, 30000U};
    uint64_t now = 80100U;
    for (size_t i = 0; i < sizeof(delays) / sizeof(delays[0]); ++i) {
        bridge_sse_disconnected(&state, now);
        TEST_ASSERT_FALSE(bridge_sse_retry_due(&state, now + delays[i] - 1U));
        TEST_ASSERT_TRUE(bridge_sse_retry_due(&state, now + delays[i]));
        now += delays[i];
    }
    bridge_sse_connected(&state, now);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
    bridge_sse_disconnected(&state, now);
    TEST_ASSERT_TRUE(bridge_sse_retry_due(&state, now + 5000U));
}

static void signals_are_coalesced_and_correlated(void)
{
    bridge_sse_t state;
    open_stream(&state);
    feed_text(&state, "event: pending_events\ndata: {\ndata: }\n\n", 110U);
    feed_text(&state, "event: pending_events\ndata: {}\n\n", 111U);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
    TEST_ASSERT_FALSE(bridge_sse_take_pending_events(&state));
    feed_text(&state, "event: assistant_result\ndata: {\"request_id\":"
              "\"12345678-1234-abcd-9876-123456789abd\"}\n\n", 112U);
    TEST_ASSERT_FALSE(bridge_sse_take_assistant_result(&state));
    feed_text(&state, ASSISTANT_FRAME, 113U);
    TEST_ASSERT_TRUE(bridge_sse_set_assistant(&state, ID));
    TEST_ASSERT_FALSE(bridge_sse_set_assistant(&state, "invalid"));
    TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(&state));
    feed_text(&state, ASSISTANT_FRAME, 114U);
    TEST_ASSERT_TRUE(bridge_sse_set_assistant(&state, OTHER_ID));
    TEST_ASSERT_FALSE(bridge_sse_take_assistant_result(&state));
    bridge_sse_disconnected(&state, 115U);
    bridge_sse_connected(&state, 5115U);
    TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(&state));
    TEST_ASSERT_TRUE(bridge_sse_set_assistant(&state, NULL));
    bridge_sse_disconnected(&state, 5116U);
    bridge_sse_connected(&state, 10116U);
    TEST_ASSERT_FALSE(bridge_sse_take_assistant_result(&state));
}

static void json_escapes_and_whitespace_are_valid(void)
{
    bridge_sse_t state;
    open_stream(&state);
    feed_text(&state, "event: assistant_result\ndata: { \"\\u0072equest_id\" : "
              "\"\\u00312345678-1234-abcd-9876-123456789abc\" }\n\n", 101U);
    TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(&state));
    feed_text(&state, "event: pending_events\ndata: {  \t}\n\n", 102U);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));

    /* The fully escaped key and UUID exceed the V4 line budget. */
    char frame[BRIDGE_SSE_LINE_MAX_BYTES + 100U];
    size_t used = (size_t)snprintf(frame, sizeof(frame), "event: assistant_result\ndata: {\"");
    const char *key = "request_id";
    for (size_t i = 0; i < strlen(key); ++i) {
        used += (size_t)snprintf(frame + used, sizeof(frame) - used, "\\u%04x", (unsigned char)key[i]);
    }
    used += (size_t)snprintf(frame + used, sizeof(frame) - used, "\":\"");
    for (size_t i = 0; i < strlen(ID); ++i) {
        used += (size_t)snprintf(frame + used, sizeof(frame) - used, "\\u%04x", (unsigned char)ID[i]);
    }
    used += (size_t)snprintf(frame + used, sizeof(frame) - used, "\"}\n\n");
    TEST_ASSERT_TRUE(used > 128U && used < sizeof(frame));
    TEST_ASSERT_TRUE(bridge_sse_feed(&state, frame, used, 103U));
    TEST_ASSERT_TRUE(bridge_sse_take_assistant_result(&state));
}

static void invalid_payloads_are_rejected(void)
{
    const char *invalid[] = {
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\",\"request_id\":\"12345678-1234-abcd-9876-123456789abc\"}",
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\",\"extra\":1}",
        "{\"request_id\\u0000suffix\":\"12345678-1234-abcd-9876-123456789abc\"}",
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\\u0000suffix\"}",
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\"} trailing",
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\"",
        "{\"request_id\":[\"12345678-1234-abcd-9876-123456789abc\"]}",
        "{\"request_id\":\"12345678-1234-ABCD-9876-123456789abc\"}",
        "{\"request_id\":\"12345678-1234-abcd-9876-123456789abc\"\001}",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        bridge_sse_t state;
        open_stream(&state);
        feed_text(&state, "event: assistant_result\ndata: ", 101U);
        feed_text(&state, invalid[i], 102U);
        TEST_ASSERT_FALSE(bridge_sse_feed(&state, "\n\n", 2U, 103U));
        TEST_ASSERT_FALSE(bridge_sse_take_assistant_result(&state));
        TEST_ASSERT_FALSE(bridge_sse_feed(&state, "\n", 1U, 104U));
    }
}

static void limits_and_utf8_are_enforced(void)
{
    bridge_sse_t state;
    open_stream(&state);
    char line[BRIDGE_SSE_LINE_MAX_BYTES + 1U];
    memset(line, 'a', sizeof(line));
    TEST_ASSERT_TRUE(bridge_sse_feed(&state, line, BRIDGE_SSE_LINE_MAX_BYTES, 101U));
    TEST_ASSERT_FALSE(bridge_sse_feed(&state, line, 1U, 102U));

    open_stream(&state);
    for (size_t i = 0; i < BRIDGE_SSE_FRAME_MAX_BYTES / 3U; ++i) {
        feed_text(&state, ":x\n", 103U);
    }
    TEST_ASSERT_FALSE(bridge_sse_feed(&state, ":x\n", 3U, 104U));

    open_stream(&state);
    const unsigned char invalid_utf8[] = {':', 0xC0U, 0x80U, '\n'};
    TEST_ASSERT_FALSE(bridge_sse_feed(&state, invalid_utf8, sizeof(invalid_utf8), 105U));
    open_stream(&state);
    const unsigned char nul[] = {':', 0, '\n'};
    TEST_ASSERT_FALSE(bridge_sse_feed(&state, nul, sizeof(nul), 106U));
    open_stream(&state);
    const unsigned char valid_utf8[] = {':', ' ', 0xF0U, 0x9FU, 0x8CU, 0x8EU, '\n', '\n'};
    for (size_t i = 0; i < sizeof(valid_utf8); ++i) {
        TEST_ASSERT_TRUE(bridge_sse_feed(&state, valid_utf8 + i, 1U, 107U));
    }
    for (size_t i = 0; i < 1000U; ++i) {
        feed_text(&state, ":heartbeat\n\n", 108U + i);
    }
}

static void disconnect_drops_partial_frame(void)
{
    bridge_sse_t state;
    open_stream(&state);
    feed_text(&state, "event: pending_events\ndata:", 101U);
    bridge_sse_disconnected(&state, 102U);
    bridge_sse_connected(&state, 5102U);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
    feed_text(&state, " {}\n\n", 5103U);
    TEST_ASSERT_FALSE(bridge_sse_take_pending_events(&state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(fragmented_frames_and_crlf);
    RUN_TEST(heartbeat_and_reconnect_timing);
    RUN_TEST(signals_are_coalesced_and_correlated);
    RUN_TEST(json_escapes_and_whitespace_are_valid);
    RUN_TEST(invalid_payloads_are_rejected);
    RUN_TEST(limits_and_utf8_are_enforced);
    RUN_TEST(disconnect_drops_partial_frame);
    return UNITY_END();
}
