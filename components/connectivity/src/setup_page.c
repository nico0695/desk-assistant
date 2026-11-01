#include "setup_page.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PAGE_TITLE "Desk Assistant"
#define PAGE_FORM_ACTION "/save"
#define PAGE_FORM_HELP "Choose the Wi-Fi network this device should join."
#define PAGE_SAVED_MESSAGE "Saved. The device is trying to connect. You can close this page."
#define PAGE_REPLACED_PREFIX "It replaced the oldest saved network: "
#define PAGE_REPLACED_SUFFIX "."
#define PAGE_INVALID_MESSAGE "The network name or password is not valid. Check them and try again."
#define PAGE_EXPIRED_MESSAGE "This page has expired. Open it again and try once more."
#define PAGE_FAILED_MESSAGE "The device could not save the network. Try again."
#define PAGE_FORM_PATH "/"
#define PAGE_BACK_LINK "Back to the form"

/* Colors of the device screen. */
#define PAGE_STYLE                                                                                                    \
    "<style>"                                                                                                         \
    "*{box-sizing:border-box}"                                                                                        \
    "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px 16px;"        \
    "background:#0a0f14;color:#edf1f2;font:17px/1.45 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}"          \
    "main{width:100%%;max-width:400px;background:#151c22;border-radius:16px;padding:28px 24px}"                      \
    "h1{margin:0 0 8px;font-size:24px}"                                                                              \
    "p{margin:0 0 20px;color:#bdc8cd}"                                                                               \
    "label{display:block;margin:0 0 6px;font-size:16px;color:#bdc8cd}"                                               \
    "input{width:100%%;margin:0 0 18px;padding:14px;border:1px solid #30424d;border-radius:12px;"                    \
    "background:#0a0f14;color:#edf1f2;font:inherit}"                                                                 \
    "input:focus{outline:2px solid #6b8796;border-color:#6b8796}"                                                    \
    ".row{display:flex;gap:8px}.row input{flex:1;min-width:0}"                                                       \
    "button{padding:14px 16px;border:0;border-radius:12px;background:#30424d;color:#edf1f2;font:inherit}"            \
    ".go{width:100%%;margin-top:4px;background:#526d7b;font-weight:600}"                                              \
    "a{color:#edf1f2}.err{color:#fca5a5}"                                                                            \
    "</style>"

#define PAGE_HEAD_FORMAT                                                                                              \
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"                                                \
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"                                       \
    "<meta name=\"color-scheme\" content=\"dark\"><title>" PAGE_TITLE "</title>" PAGE_STYLE "</head><body><main>"

#define PAGE_TAIL "</main></body></html>"

#define PAGE_FORM_FORMAT                                                                                              \
    "<h1>" PAGE_TITLE "</h1><p>" PAGE_FORM_HELP "</p>"                                                               \
    "<form method=\"post\" action=\"" PAGE_FORM_ACTION "\" autocomplete=\"off\">"                                    \
    "<input type=\"hidden\" name=\"nonce\" value=\"%s\">"                                                            \
    "<label for=\"ssid\">Network name</label>"                                                                       \
    "<input id=\"ssid\" name=\"ssid\" required maxlength=\"%u\" autocapitalize=\"none\" "           \
    "autocorrect=\"off\" spellcheck=\"false\">"                                                                      \
    "<label for=\"password\">Password</label>"                                                                       \
    "<div class=\"row\"><input id=\"password\" name=\"password\" type=\"password\" maxlength=\"%u\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\">"                       \
    "<button type=\"button\" id=\"show\" hidden>Show</button></div>"                                                 \
    "<button class=\"go\" type=\"submit\">Save and connect</button></form>"                                          \
    "<script>var b=document.getElementById('show'),p=document.getElementById('password');b.hidden=false;"           \
    "b.onclick=function(){var s=p.type==='password';p.type=s?'text':'password';b.textContent=s?'Hide':'Show'}"      \
    "</script>"

/* Writes text after position, escaping what HTML treats specially; false if it does not fit. */
static bool append_escaped(char *out, size_t capacity, size_t *position, const char *text)
{
    for (; *text != '\0'; ++text) {
        const char *replacement = NULL;
        switch (*text) {
        case '&':
            replacement = "&amp;";
            break;
        case '<':
            replacement = "&lt;";
            break;
        case '>':
            replacement = "&gt;";
            break;
        case '"':
            replacement = "&quot;";
            break;
        case '\'':
            replacement = "&#39;";
            break;
        default:
            break;
        }
        const size_t length = replacement != NULL ? strlen(replacement) : 1U;
        if (*position + length >= capacity) {
            return false;
        }
        memcpy(&out[*position], replacement != NULL ? replacement : text, length);
        *position += length;
    }
    out[*position] = '\0';
    return true;
}

/* Appends formatted text after position; false if it does not fit. */
static bool append(char *out, size_t capacity, size_t *position, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(&out[*position], capacity - *position, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= capacity - *position) {
        return false;
    }
    *position += (size_t)length;
    return true;
}

static int finish(bool fitted, size_t position)
{
    return fitted ? (int)position : -1;
}

int setup_page_form(char *out, size_t capacity, const char *nonce)
{
    size_t position = 0;
    const bool fitted = capacity > 0 && append(out, capacity, &position, PAGE_HEAD_FORMAT) &&
                        append(out, capacity, &position, PAGE_FORM_FORMAT, nonce, (unsigned)WIFI_SSID_MAX_BYTES,
                               (unsigned)WIFI_PASSWORD_HEX_KEY_BYTES) &&
                        append(out, capacity, &position, PAGE_TAIL);
    return finish(fitted, position);
}

static const char *result_message(setup_page_result_t result)
{
    switch (result) {
    case SETUP_PAGE_SAVED:
    case SETUP_PAGE_SAVED_REPLACED:
        return PAGE_SAVED_MESSAGE;
    case SETUP_PAGE_INVALID:
        return PAGE_INVALID_MESSAGE;
    case SETUP_PAGE_EXPIRED:
        return PAGE_EXPIRED_MESSAGE;
    case SETUP_PAGE_FAILED:
    default:
        return PAGE_FAILED_MESSAGE;
    }
}

static bool result_is_error(setup_page_result_t result)
{
    return result != SETUP_PAGE_SAVED && result != SETUP_PAGE_SAVED_REPLACED;
}

int setup_page_result(char *out, size_t capacity, setup_page_result_t result, const char *replaced_ssid)
{
    size_t position = 0;
    const bool error = result_is_error(result);
    bool fitted = capacity > 0 && append(out, capacity, &position, PAGE_HEAD_FORMAT) &&
                  append(out, capacity, &position, "<h1>" PAGE_TITLE "</h1><p%s>%s</p>", error ? " class=\"err\"" : "",
                         result_message(result));
    if (fitted && result == SETUP_PAGE_SAVED_REPLACED) {
        fitted = append(out, capacity, &position, "<p>" PAGE_REPLACED_PREFIX) &&
                 append_escaped(out, capacity, &position, replaced_ssid != NULL ? replaced_ssid : "") &&
                 append(out, capacity, &position, PAGE_REPLACED_SUFFIX "</p>");
    }
    if (fitted && error) {
        fitted = append(out, capacity, &position, "<p><a href=\"" PAGE_FORM_PATH "\">" PAGE_BACK_LINK "</a></p>");
    }
    fitted = fitted && append(out, capacity, &position, PAGE_TAIL);
    return finish(fitted, position);
}
