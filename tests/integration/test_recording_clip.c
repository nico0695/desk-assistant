#include <stdlib.h>
#include <string.h>

#include "esp_partition.h"
#include "freertos/task.h"
#include "recording.h"
#include "recording_clip.h"
#include "unity.h"

#define FLASH_BYTES 0x100000U
#define SECTOR_BYTES 4096U

/* NOR-like fake: erase sets 0xff and a write may only program erased bytes. */
static uint8_t *flash;
static esp_partition_t fake = {.type = ESP_PARTITION_TYPE_DATA, .subtype = 0x40, .size = FLASH_BYTES,
                               .erase_size = SECTOR_BYTES, .label = "audio_tmp"};
static bool partition_present;
static unsigned erases, erased_bytes, reads;
static bool fail_read, fail_write, fail_erase;

const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label)
{
    TEST_ASSERT_EQUAL_INT(ESP_PARTITION_TYPE_DATA, type);
    TEST_ASSERT_EQUAL_INT(0x40, subtype);
    TEST_ASSERT_EQUAL_STRING("audio_tmp", label);
    return partition_present ? &fake : NULL;
}

esp_err_t esp_partition_read(const esp_partition_t *partition, size_t offset, void *destination, size_t size)
{
    TEST_ASSERT_TRUE(partition == &fake && offset + size <= FLASH_BYTES);
    ++reads;
    if (fail_read) return ESP_FAIL;
    memcpy(destination, flash + offset, size);
    return ESP_OK;
}

esp_err_t esp_partition_write(const esp_partition_t *partition, size_t offset, const void *source, size_t size)
{
    TEST_ASSERT_TRUE(partition == &fake && offset + size <= FLASH_BYTES);
    for (size_t i = 0; i < size; ++i) TEST_ASSERT_EQUAL_HEX8(0xff, flash[offset + i]);
    if (fail_write) {
        flash[offset] = 0;
        return ESP_FAIL;
    }
    memcpy(flash + offset, source, size);
    return ESP_OK;
}

esp_err_t esp_partition_erase_range(const esp_partition_t *partition, size_t offset, size_t size)
{
    TEST_ASSERT_TRUE(partition == &fake && offset + size <= FLASH_BYTES);
    TEST_ASSERT_EQUAL_UINT(0, offset % SECTOR_BYTES);
    TEST_ASSERT_EQUAL_UINT(0, size % SECTOR_BYTES);
    ++erases;
    if (fail_erase) return ESP_FAIL;
    memset(flash + offset, 0xff, size);
    erased_bytes += (unsigned)size;
    return ESP_OK;
}

void vTaskDelay(TickType_t ticks) { (void)ticks; }

void setUp(void)
{
    flash = malloc(FLASH_BYTES);
    memset(flash, 0xff, FLASH_BYTES);
    partition_present = true;
    fake.encrypted = fake.readonly = false;
    erases = erased_bytes = reads = 0;
    fail_read = fail_write = fail_erase = false;
}

void tearDown(void) { free(flash); }

static void fill_samples(int16_t *samples, size_t count, int16_t first)
{
    for (size_t i = 0; i < count; ++i) samples[i] = (int16_t)(first + (int16_t)i);
}

static void record_samples(size_t count)
{
    int16_t block[256];
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_begin());
    for (size_t done = 0; done < count;) {
        size_t step = count - done < 256U ? count - done : 256U;
        fill_samples(block, step, (int16_t)done);
        TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_append(block, step));
        done += step;
    }
}

static uint32_t u32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static void test_invalid_partition_is_rejected(void)
{
    partition_present = false;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, recording_clip_init());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_clear());
    partition_present = true;
    fake.encrypted = true;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, recording_clip_init());
}

static void test_boot_scan_erases_only_residue_and_begin_needs_a_clear(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_init());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_begin());
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_EQUAL_UINT(0, erases);

    flash[3 * SECTOR_BYTES + 10] = 0x12;
    flash[5 * SECTOR_BYTES + 1] = 0x34;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_init());
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_EQUAL_UINT(1, erases);
    TEST_ASSERT_EQUAL_UINT(3 * SECTOR_BYTES, erased_bytes);
    TEST_ASSERT_EQUAL_HEX8(0xff, flash[5 * SECTOR_BYTES + 1]);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_begin());
}

static void test_wav_header_and_reads_across_header_and_pcm(void)
{
    recording_clip_init();
    recording_clip_clear();
    TEST_ASSERT_EQUAL_UINT(0, recording_clip_wav_size());
    record_samples(1000);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_finish());
    const size_t size = RECORDING_WAV_HEADER_BYTES + 2000U;
    TEST_ASSERT_EQUAL_UINT(size, recording_clip_wav_size());

    uint8_t wav[RECORDING_WAV_HEADER_BYTES + 2000U];
    size_t count = 0, offset = 0;
    /* Odd chunk sizes cross the header boundary and leave unaligned PCM offsets. */
    while (offset < size) {
        TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_read(offset, wav + offset, 7, &count));
        TEST_ASSERT_GREATER_THAN_UINT(0, count);
        offset += count;
    }
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_read(size, wav, sizeof(wav), &count));
    TEST_ASSERT_EQUAL_UINT(0, count);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, recording_clip_read(size + 1U, wav, sizeof(wav), &count));

    TEST_ASSERT_EQUAL_MEMORY("RIFF", wav, 4);
    TEST_ASSERT_EQUAL_UINT32(size - 8U, u32le(wav + 4));
    TEST_ASSERT_EQUAL_MEMORY("WAVEfmt ", wav + 8, 8);
    TEST_ASSERT_EQUAL_UINT8(1, wav[20]);
    TEST_ASSERT_EQUAL_UINT8(1, wav[22]);
    TEST_ASSERT_EQUAL_UINT32(16000, u32le(wav + 24));
    TEST_ASSERT_EQUAL_UINT32(32000, u32le(wav + 28));
    TEST_ASSERT_EQUAL_UINT8(16, wav[34]);
    TEST_ASSERT_EQUAL_MEMORY("data", wav + 36, 4);
    TEST_ASSERT_EQUAL_UINT32(2000, u32le(wav + 40));
    int16_t expected[1000];
    fill_samples(expected, 1000, 0);
    TEST_ASSERT_EQUAL_MEMORY(expected, wav + RECORDING_WAV_HEADER_BYTES, sizeof(expected));
}

static void test_append_is_bounded_to_the_maximum_clip(void)
{
    recording_clip_init();
    recording_clip_clear();
    record_samples(RECORDING_MAX_SAMPLES);
    int16_t extra = 1;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_SIZE, recording_clip_append(&extra, 1));
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_finish());
    TEST_ASSERT_EQUAL_UINT(RECORDING_MAX_PCM_BYTES + RECORDING_WAV_HEADER_BYTES, recording_clip_wav_size());
}

static void test_clear_after_an_attempt_erases_only_the_written_range(void)
{
    recording_clip_init();
    recording_clip_clear();
    record_samples(3000);
    recording_clip_finish();
    reads = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_EQUAL_UINT(0, reads);
    TEST_ASSERT_EQUAL_UINT(2 * SECTOR_BYTES, erased_bytes);
    TEST_ASSERT_EQUAL_UINT(0, recording_clip_wav_size());
    for (size_t i = 0; i < 6000U; ++i) TEST_ASSERT_EQUAL_HEX8(0xff, flash[i]);
    erases = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_EQUAL_UINT(0, erases);
}

static void test_failed_write_blocks_and_clear_covers_the_partial_write(void)
{
    recording_clip_init();
    recording_clip_clear();
    record_samples(256);
    int16_t block[256] = {0};
    fail_write = true;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, recording_clip_append(block, 256));
    fail_write = false;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_append(block, 256));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_finish());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_begin());
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_EQUAL_HEX8(0xff, flash[512]);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_begin());
}

static void test_failed_read_or_erase_blocks_until_a_full_scan_clears(void)
{
    recording_clip_init();
    recording_clip_clear();
    record_samples(1000);
    recording_clip_finish();
    uint8_t buffer[64];
    size_t count = 1;
    fail_read = true;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, recording_clip_read(40, buffer, sizeof(buffer), &count));
    TEST_ASSERT_EQUAL_UINT(0, count);
    fail_read = false;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_read(0, buffer, sizeof(buffer), &count));

    fail_erase = true;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, recording_clip_clear());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_clip_begin());
    fail_erase = false;
    reads = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_clear());
    TEST_ASSERT_GREATER_THAN_UINT(0, reads);
    TEST_ASSERT_EQUAL_HEX8(0xff, flash[0]);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_clip_begin());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_invalid_partition_is_rejected);
    RUN_TEST(test_boot_scan_erases_only_residue_and_begin_needs_a_clear);
    RUN_TEST(test_wav_header_and_reads_across_header_and_pcm);
    RUN_TEST(test_append_is_bounded_to_the_maximum_clip);
    RUN_TEST(test_clear_after_an_attempt_erases_only_the_written_range);
    RUN_TEST(test_failed_write_blocks_and_clear_covers_the_partial_write);
    RUN_TEST(test_failed_read_or_erase_blocks_until_a_full_scan_clears);
    return UNITY_END();
}
