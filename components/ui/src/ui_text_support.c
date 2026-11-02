#include "ui_text_support.h"

#include <stddef.h>
#include <stdint.h>

/* Decodes one UTF-8 character and advances text; the bridge codec already rejected invalid UTF-8. */
static uint32_t next_codepoint(const char **text)
{
    const unsigned char *at = (const unsigned char *)*text;
    uint32_t codepoint = *at++;
    size_t continuation = codepoint >= 0xF0U ? 3U : codepoint >= 0xE0U ? 2U : codepoint >= 0xC0U ? 1U : 0U;
    if (continuation > 0U) {
        codepoint &= 0x3FU >> continuation;
    }
    for (; continuation > 0U && (*at & 0xC0U) == 0x80U; --continuation) {
        codepoint = (codepoint << 6) | (*at++ & 0x3FU);
    }
    *text = (const char *)at;
    return codepoint;
}

bool ui_text_support_covers(const lv_font_t *font, const char *text)
{
    while (*text != '\0') {
        const uint32_t letter = next_codepoint(&text);
        if (letter == '\n' || letter == '\r' || letter == '\t') {
            continue;
        }
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, letter, 0) || glyph.is_placeholder) {
            return false;
        }
    }
    return true;
}
