/* Writes the phone setup pages, with sample data, as HTML files. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "setup_page.h"

#define SAMPLE_NONCE "0123456789abcdef0123456789abcdef"
#define SAMPLE_REPLACED_SSID "Old <Office> & Co"

static char page[SETUP_PAGE_MAX_BYTES];

static void write_page(const char *directory, const char *name, int length)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.html", directory, name);
    FILE *file = length > 0 ? fopen(path, "wb") : NULL;
    const bool written = file != NULL && fwrite(page, 1, (size_t)length, file) == (size_t)length;
    if (file != NULL) {
        fclose(file);
    }
    if (!written) {
        fprintf(stderr, "Could not write %s\n", path);
        exit(EXIT_FAILURE);
    }
    printf("%s (%d bytes)\n", path, length);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *directory = argv[1];
    write_page(directory, "form", setup_page_form(page, sizeof(page), SAMPLE_NONCE));
    write_page(directory, "saved", setup_page_result(page, sizeof(page), SETUP_PAGE_SAVED, NULL));
    write_page(directory, "saved-replaced",
               setup_page_result(page, sizeof(page), SETUP_PAGE_SAVED_REPLACED, SAMPLE_REPLACED_SSID));
    write_page(directory, "invalid", setup_page_result(page, sizeof(page), SETUP_PAGE_INVALID, NULL));
    write_page(directory, "expired", setup_page_result(page, sizeof(page), SETUP_PAGE_EXPIRED, NULL));
    write_page(directory, "failed", setup_page_result(page, sizeof(page), SETUP_PAGE_FAILED, NULL));
    return EXIT_SUCCESS;
}
