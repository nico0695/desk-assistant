#include "wifi_setup.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "network_store.h"
#include "sdkconfig.h"
#include "setup_credentials.h"
#include "setup_form.h"
#include "setup_page.h"
#include "wifi_list.h"

#define IPV4_MAPPED_PREFIX_ZERO_BYTES 10U
#define IPV4_MAPPED_MARKER 0xffU
#define IPV4_OFFSET_IN_MAPPED_BYTES 12U
#define IPV4_BYTES 4U
#define HTTPD_MAX_URI_HANDLERS 2U
#define HTTPD_MAX_OPEN_SOCKETS 3U

typedef struct {
    char ssid[SETUP_AP_SSID_SIZE];
    char password[SETUP_AP_PASSWORD_SIZE];
    char nonce[SETUP_NONCE_SIZE];
    /* Form body plus the terminator the parser needs. */
    char body[SETUP_FORM_MAX_BYTES + 1U];
    char page[SETUP_PAGE_MAX_BYTES];
} setup_session_t;

static const char *TAG = "setup";
static setup_session_t *session;
static httpd_handle_t server;
static esp_netif_t *access_point_netif;
static wifi_setup_saved_t on_saved;

static bool ipv4_of(const struct sockaddr_storage *address, uint32_t *ipv4)
{
    if (address->ss_family == AF_INET) {
        *ipv4 = ((const struct sockaddr_in *)address)->sin_addr.s_addr;
        return true;
    }
    if (address->ss_family != AF_INET6) {
        return false;
    }
    /* The server reports IPv4 clients of its IPv6 sockets as IPv4-mapped addresses. */
    const uint8_t *bytes = ((const struct sockaddr_in6 *)address)->sin6_addr.un.u8_addr;
    for (unsigned i = 0; i < IPV4_MAPPED_PREFIX_ZERO_BYTES; ++i) {
        if (bytes[i] != 0U) {
            return false;
        }
    }
    if (bytes[IPV4_MAPPED_PREFIX_ZERO_BYTES] != IPV4_MAPPED_MARKER ||
        bytes[IPV4_MAPPED_PREFIX_ZERO_BYTES + 1U] != IPV4_MAPPED_MARKER) {
        return false;
    }
    memcpy(ipv4, &bytes[IPV4_OFFSET_IN_MAPPED_BYTES], IPV4_BYTES);
    return true;
}

/* Only a phone on the setup access point may use the page, not another host that reached the device. */
static bool request_from_setup_network(httpd_req_t *request)
{
    const int socket_fd = httpd_req_to_sockfd(request);
    struct sockaddr_storage local;
    struct sockaddr_storage peer;
    socklen_t local_length = sizeof(local);
    socklen_t peer_length = sizeof(peer);
    esp_netif_ip_info_t access_point;
    uint32_t local_ipv4;
    uint32_t peer_ipv4;
    return socket_fd >= 0 && getsockname(socket_fd, (struct sockaddr *)&local, &local_length) == 0 &&
           getpeername(socket_fd, (struct sockaddr *)&peer, &peer_length) == 0 && ipv4_of(&local, &local_ipv4) &&
           ipv4_of(&peer, &peer_ipv4) && esp_netif_get_ip_info(access_point_netif, &access_point) == ESP_OK &&
           setup_peer_allowed(local_ipv4, peer_ipv4, access_point.ip.addr, access_point.netmask.addr);
}

static esp_err_t send_page(httpd_req_t *request, const char *status, int length)
{
    if (length < 0) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
    }
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, session->page, (ssize_t)length);
}

static esp_err_t send_result(httpd_req_t *request, setup_page_result_t result, const char *replaced_ssid)
{
    const char *status = HTTPD_200;
    if (result == SETUP_PAGE_FAILED) {
        status = HTTPD_500;
    } else if (result == SETUP_PAGE_INVALID || result == SETUP_PAGE_EXPIRED) {
        status = HTTPD_400;
    }
    return send_page(request, status, setup_page_result(session->page, sizeof(session->page), result, replaced_ssid));
}

static esp_err_t handle_form(httpd_req_t *request)
{
    if (!request_from_setup_network(request)) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, NULL);
    }
    return send_page(request, HTTPD_200, setup_page_form(session->page, sizeof(session->page), session->nonce));
}

/* Reads the whole body into the session; false if it is empty, too long or incomplete. */
static bool read_body(httpd_req_t *request, size_t *length)
{
    const size_t expected = request->content_len;
    if (expected == 0U || expected > SETUP_FORM_MAX_BYTES) {
        return false;
    }
    size_t received = 0;
    while (received < expected) {
        const int count = httpd_req_recv(request, &session->body[received], expected - received);
        if (count <= 0) {
            return false;
        }
        received += (size_t)count;
    }
    session->body[received] = '\0';
    *length = received;
    return true;
}

static esp_err_t save_submitted_network(httpd_req_t *request)
{
    size_t length;
    if (!read_body(request, &length)) {
        return send_result(request, SETUP_PAGE_INVALID, NULL);
    }
    setup_form_t form;
    const setup_form_result_t parsed = setup_form_parse(session->body, length, session->nonce, &form);
    if (parsed == SETUP_FORM_STALE_NONCE) {
        return send_result(request, SETUP_PAGE_EXPIRED, NULL);
    }
    if (parsed != SETUP_FORM_OK || !wifi_ssid_valid(form.ssid) || !wifi_password_valid(form.password)) {
        return send_result(request, SETUP_PAGE_INVALID, NULL);
    }
    char replaced_ssid[WIFI_SSID_SIZE];
    const esp_err_t saved = network_store_save(form.ssid, form.password, replaced_ssid);
    if (saved != ESP_OK) {
        return send_result(request, SETUP_PAGE_FAILED, NULL);
    }
    /* The response goes out before the attempt on the new network, which can move the access point's channel. */
    const esp_err_t sent = send_result(request, replaced_ssid[0] != '\0' ? SETUP_PAGE_SAVED_REPLACED : SETUP_PAGE_SAVED,
                                       replaced_ssid);
    ESP_LOGI(TAG, "network saved from the phone");
    on_saved();
    return sent;
}

static esp_err_t handle_save(httpd_req_t *request)
{
    if (!request_from_setup_network(request)) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, NULL);
    }
    const esp_err_t result = save_submitted_network(request);
    memset(session->body, 0, sizeof(session->body));
    return result;
}

static esp_err_t start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = CONFIG_CONNECTIVITY_HTTPD_STACK_SIZE;
    config.task_priority = CONFIG_CONNECTIVITY_HTTPD_PRIORITY;
    config.max_uri_handlers = HTTPD_MAX_URI_HANDLERS;
    config.max_open_sockets = HTTPD_MAX_OPEN_SOCKETS;
    config.lru_purge_enable = true;
    const httpd_uri_t form_uri = {.uri = "/", .method = HTTP_GET, .handler = handle_form};
    const httpd_uri_t save_uri = {.uri = "/save", .method = HTTP_POST, .handler = handle_save};
    esp_err_t error = httpd_start(&server, &config);
    if (error == ESP_OK) {
        error = httpd_register_uri_handler(server, &form_uri);
    }
    if (error == ESP_OK) {
        error = httpd_register_uri_handler(server, &save_uri);
    }
    if (error != ESP_OK && server != NULL) {
        httpd_stop(server);
        server = NULL;
    }
    return error;
}

static esp_err_t start_access_point(void)
{
    if (access_point_netif == NULL) {
        access_point_netif = esp_netif_create_default_wifi_ap();
        if (access_point_netif == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    wifi_config_t config = {0};
    config.ap.ssid_len = (uint8_t)strlen(session->ssid);
    memcpy(config.ap.ssid, session->ssid, config.ap.ssid_len);
    memcpy(config.ap.password, session->password, strlen(session->password));
    config.ap.channel = SETUP_AP_CHANNEL;
    config.ap.max_connection = SETUP_AP_MAX_CLIENTS;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    esp_err_t error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (error == ESP_OK) {
        error = esp_wifi_set_config(WIFI_IF_AP, &config);
    }
    memset(&config, 0, sizeof(config));
    return error;
}

static uint32_t random_bits(void)
{
    return esp_random();
}

static void release_session(void)
{
    memset(session, 0, sizeof(*session));
    free(session);
    session = NULL;
}

esp_err_t wifi_setup_open(wifi_setup_saved_t saved_callback)
{
    if (session != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    session = calloc(1, sizeof(*session));
    if (session == NULL) {
        return ESP_ERR_NO_MEM;
    }
    uint8_t base_mac[6];
    esp_err_t error = esp_efuse_mac_get_default(base_mac);
    if (error == ESP_OK) {
        setup_ap_ssid(base_mac, session->ssid);
        setup_ap_password(random_bits, session->password);
        setup_nonce(random_bits, session->nonce);
        on_saved = saved_callback;
        error = start_access_point();
    }
    if (error == ESP_OK) {
        error = start_server();
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "setup not opened: %s", esp_err_to_name(error));
        wifi_setup_close();
        return error;
    }
    ESP_LOGI(TAG, "setup opened");
    return ESP_OK;
}

void wifi_setup_close(void)
{
    if (session == NULL) {
        return;
    }
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
    }
    const esp_err_t error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "access point not stopped: %s", esp_err_to_name(error));
    }
    release_session();
    ESP_LOGI(TAG, "setup closed");
}

bool wifi_setup_active(void)
{
    return session != NULL;
}

void wifi_setup_fill_view(connectivity_view_t *view)
{
    view->setup_active = session != NULL;
    if (session != NULL) {
        memcpy(view->setup_ssid, session->ssid, sizeof(view->setup_ssid));
        memcpy(view->setup_password, session->password, sizeof(view->setup_password));
    }
}
