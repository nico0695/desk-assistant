#include <string.h>
#include "bridge_origin_store.h"
#include "storage.h"
#include "unity.h"

static bridge_origin_t persisted;
static bool write_fails;
static unsigned callbacks;

esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size)
{
    (void)name_space; (void)key;
    TEST_ASSERT_EQUAL_UINT(sizeof(persisted), size);
    if (write_fails) return ESP_FAIL;
    memcpy(&persisted, data, size);
    return ESP_OK;
}

esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size)
{
    (void)name_space; (void)key;
    memcpy(data, &persisted, size);
    return ESP_OK;
}

static void changed(const bridge_origin_t *origin)
{
    ++callbacks;
    TEST_ASSERT_EQUAL_STRING(persisted.url, origin->url);
    TEST_ASSERT_EQUAL_STRING(persisted.token, origin->token);
}

void setUp(void) { memset(&persisted, 0, sizeof(persisted)); write_fails = false; callbacks = 0; bridge_origin_set_changed_callback(changed); }
void tearDown(void) { bridge_origin_set_changed_callback(NULL); }

static void notify_only_after_successful_persistence(void)
{
    const char *token = "01234567890123456789012345678901";
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, bridge_origin_save("http://example.test", token));
    TEST_ASSERT_EQUAL_UINT(0, callbacks);
    write_fails = true;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, bridge_origin_save("https://example.test", token));
    TEST_ASSERT_EQUAL_UINT(0, callbacks);
    write_fails = false;
    TEST_ASSERT_EQUAL_INT(ESP_OK, bridge_origin_save("https://example.test", token));
    TEST_ASSERT_EQUAL_UINT(1, callbacks);
}

int main(void) { UNITY_BEGIN(); RUN_TEST(notify_only_after_successful_persistence); return UNITY_END(); }
