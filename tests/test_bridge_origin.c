#include <string.h>
#include "bridge_origin.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_accepts_https_hosts_ports_and_path_prefixes(void)
{
    TEST_ASSERT_TRUE(bridge_url_valid("https://bridge.example.com"));
    TEST_ASSERT_TRUE(bridge_url_valid("https://bridge.example.com/"));
    TEST_ASSERT_TRUE(bridge_url_valid("https://bridge.example.com./prefix/v1"));
    TEST_ASSERT_TRUE(bridge_url_valid("https://my-host:8443/a%2Fb"));
    TEST_ASSERT_TRUE(bridge_url_valid("https://192.168.1.10:65535"));
    TEST_ASSERT_TRUE(bridge_url_valid("https://localhost"));
}

static void test_rejects_other_schemes_userinfo_query_and_fragment(void)
{
    TEST_ASSERT_FALSE(bridge_url_valid("http://bridge.example.com"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://"));
    TEST_ASSERT_FALSE(bridge_url_valid("https:///path"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://user:pass@host"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host/path?x=1"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host/path#frag"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host/a b"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host\\path"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host/%2"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host/%zz"));
    TEST_ASSERT_FALSE(bridge_url_valid(NULL));
}

static void test_rejects_invalid_hosts_and_ports(void)
{
    TEST_ASSERT_FALSE(bridge_url_valid("https://-host.com"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host-.com"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://a..b"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://ho_st.com"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://256.1.1.1"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://1.2.3"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://01.2.3.4"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://1.2.3.4."));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host:"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host:0"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host:65536"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://host:8a"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://:443"));
    TEST_ASSERT_FALSE(bridge_url_valid("https://[::1]"));
}

static void test_url_is_at_most_256_bytes(void)
{
    char url[BRIDGE_URL_SIZE + 1];
    strcpy(url, "https://host/");
    const size_t prefix = strlen(url);
    memset(url + prefix, 'p', BRIDGE_URL_MAX_BYTES - prefix);
    url[BRIDGE_URL_MAX_BYTES] = '\0';
    TEST_ASSERT_TRUE(bridge_url_valid(url));
    url[BRIDGE_URL_MAX_BYTES] = 'p';
    url[BRIDGE_URL_MAX_BYTES + 1] = '\0';
    TEST_ASSERT_FALSE(bridge_url_valid(url));
}

static void test_token_is_32_to_256_printable_characters_without_spaces(void)
{
    char token[BRIDGE_TOKEN_SIZE + 1];
    memset(token, 'T', sizeof(token));
    token[BRIDGE_TOKEN_MIN_BYTES] = '\0';
    TEST_ASSERT_TRUE(bridge_token_valid(token));
    token[BRIDGE_TOKEN_MIN_BYTES - 1] = '\0';
    TEST_ASSERT_FALSE(bridge_token_valid(token));
    memset(token, 'T', sizeof(token));
    token[BRIDGE_TOKEN_MAX_BYTES] = '\0';
    TEST_ASSERT_TRUE(bridge_token_valid(token));
    token[BRIDGE_TOKEN_MAX_BYTES] = 'T';
    token[BRIDGE_TOKEN_MAX_BYTES + 1] = '\0';
    TEST_ASSERT_FALSE(bridge_token_valid(token));
    TEST_ASSERT_FALSE(bridge_token_valid("abcdefghijklmnop abcdefghijklmnop"));
    TEST_ASSERT_FALSE(bridge_token_valid(""));
    TEST_ASSERT_FALSE(bridge_token_valid(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_https_hosts_ports_and_path_prefixes);
    RUN_TEST(test_rejects_other_schemes_userinfo_query_and_fragment);
    RUN_TEST(test_rejects_invalid_hosts_and_ports);
    RUN_TEST(test_url_is_at_most_256_bytes);
    RUN_TEST(test_token_is_32_to_256_printable_characters_without_spaces);
    return UNITY_END();
}
