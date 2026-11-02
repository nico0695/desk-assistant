/* Adapted from main/audio_clip.c of the author's previous firmware (V4); see THIRD_PARTY_NOTICES.md. */
#include "recording_clip.h"

#include <string.h>

#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "recording.h"

#define CLIP_PARTITION_LABEL "audio_tmp"
#define CLIP_PARTITION_SUBTYPE 0x40
#define CLIP_SCAN_CHUNK_BYTES 512U
/* The scan yields this often so lower-priority tasks keep running during a full-partition scan. */
#define CLIP_SCAN_YIELD_BYTES 32768U
#define ERASED_BYTE 0xffU
#define WAV_FMT_CHUNK_BYTES 16U
#define WAV_FORMAT_PCM 1U
#define WAV_CHANNELS 1U
#define WAV_BITS_PER_SAMPLE 16U
#define WAV_RIFF_PREAMBLE_BYTES 8U

typedef enum { CLIP_BLOCKED, CLIP_EMPTY, CLIP_RECORDING, CLIP_FINISHED } clip_state_t;

static const esp_partition_t *partition;
static clip_state_t state = CLIP_BLOCKED;
static size_t pcm_bytes;
/* Upper bound of the bytes that may hold audio; meaningful only while extent_known. */
static size_t dirty_bytes;
static bool extent_known;

static void block(void)
{
    state = CLIP_BLOCKED;
    pcm_bytes = 0;
    extent_known = false;
}

esp_err_t recording_clip_init(void)
{
    block();
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, CLIP_PARTITION_SUBTYPE, CLIP_PARTITION_LABEL);
    if (partition == NULL || partition->size < RECORDING_MAX_PCM_BYTES || partition->erase_size == 0U ||
        partition->size % partition->erase_size != 0U || partition->encrypted || partition->readonly) {
        partition = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1U) / alignment * alignment;
}

/* Finds the aligned range [*first, *end) that holds non-erased bytes; empty when *first == *end. */
static esp_err_t scan_written_range(size_t *first, size_t *end)
{
    uint8_t chunk[CLIP_SCAN_CHUNK_BYTES];
    size_t lowest = partition->size, highest = 0;
    for (size_t offset = 0; offset < partition->size; offset += sizeof(chunk)) {
        esp_err_t error = esp_partition_read(partition, offset, chunk, sizeof(chunk));
        if (error != ESP_OK) {
            memset(chunk, 0, sizeof(chunk));
            return error;
        }
        for (size_t i = 0; i < sizeof(chunk); ++i) {
            if (chunk[i] != ERASED_BYTE) {
                if (lowest == partition->size) lowest = offset + i;
                highest = offset + i + 1U;
            }
        }
        if ((offset + sizeof(chunk)) % CLIP_SCAN_YIELD_BYTES == 0U) {
            vTaskDelay(1);
        }
    }
    memset(chunk, 0, sizeof(chunk));
    if (highest == 0U) {
        *first = *end = 0;
    } else {
        *first = lowest - lowest % partition->erase_size;
        *end = align_up(highest, partition->erase_size);
    }
    return ESP_OK;
}

esp_err_t recording_clip_clear(void)
{
    if (partition == NULL) return ESP_ERR_INVALID_STATE;
    size_t first = 0, end = 0;
    if (extent_known) {
        end = align_up(dirty_bytes, partition->erase_size);
    } else {
        esp_err_t error = scan_written_range(&first, &end);
        if (error != ESP_OK) {
            block();
            return error;
        }
    }
    block();
    if (end > first) {
        esp_err_t error = esp_partition_erase_range(partition, first, end - first);
        if (error != ESP_OK) return error;
    }
    dirty_bytes = 0;
    extent_known = true;
    state = CLIP_EMPTY;
    return ESP_OK;
}

esp_err_t recording_clip_begin(void)
{
    if (state != CLIP_EMPTY) return ESP_ERR_INVALID_STATE;
    pcm_bytes = 0;
    state = CLIP_RECORDING;
    return ESP_OK;
}

esp_err_t recording_clip_append(const int16_t *samples, size_t count)
{
    if (state != CLIP_RECORDING) return ESP_ERR_INVALID_STATE;
    if (samples == NULL || count == 0U) return ESP_ERR_INVALID_ARG;
    if (count > (RECORDING_MAX_PCM_BYTES - pcm_bytes) / RECORDING_BYTES_PER_SAMPLE) return ESP_ERR_INVALID_SIZE;
    const size_t bytes = count * RECORDING_BYTES_PER_SAMPLE;
    /* A failed write may still have programmed part of the range. */
    if (pcm_bytes + bytes > dirty_bytes) dirty_bytes = pcm_bytes + bytes;
    /* Little-endian PCM16 as stored by the CPU; the partition is not encrypted, so writes need no alignment. */
    esp_err_t error = esp_partition_write(partition, pcm_bytes, samples, bytes);
    if (error != ESP_OK) {
        state = CLIP_BLOCKED;
        return error;
    }
    pcm_bytes += bytes;
    return ESP_OK;
}

esp_err_t recording_clip_finish(void)
{
    if (state != CLIP_RECORDING || pcm_bytes == 0U) return ESP_ERR_INVALID_STATE;
    state = CLIP_FINISHED;
    return ESP_OK;
}

static void put_u16le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32le(uint8_t *out, uint32_t value)
{
    put_u16le(out, (uint16_t)value);
    put_u16le(out + 2, (uint16_t)(value >> 16));
}

static void wav_header(uint8_t header[RECORDING_WAV_HEADER_BYTES])
{
    const uint16_t block_align = WAV_CHANNELS * RECORDING_BYTES_PER_SAMPLE;
    memcpy(header, "RIFF", 4);
    put_u32le(header + 4, (uint32_t)(pcm_bytes + RECORDING_WAV_HEADER_BYTES - WAV_RIFF_PREAMBLE_BYTES));
    memcpy(header + 8, "WAVEfmt ", 8);
    put_u32le(header + 16, WAV_FMT_CHUNK_BYTES);
    put_u16le(header + 20, WAV_FORMAT_PCM);
    put_u16le(header + 22, WAV_CHANNELS);
    put_u32le(header + 24, RECORDING_SAMPLE_RATE_HZ);
    put_u32le(header + 28, RECORDING_SAMPLE_RATE_HZ * block_align);
    put_u16le(header + 32, block_align);
    put_u16le(header + 34, WAV_BITS_PER_SAMPLE);
    memcpy(header + 36, "data", 4);
    put_u32le(header + 40, (uint32_t)pcm_bytes);
}

size_t recording_clip_wav_size(void)
{
    return state == CLIP_FINISHED ? pcm_bytes + RECORDING_WAV_HEADER_BYTES : 0U;
}

esp_err_t recording_clip_read(size_t wav_offset, void *buffer, size_t capacity, size_t *count)
{
    if (count != NULL) *count = 0;
    if (count == NULL || (buffer == NULL && capacity != 0U)) return ESP_ERR_INVALID_ARG;
    if (state != CLIP_FINISHED) return ESP_ERR_INVALID_STATE;
    const size_t size = recording_clip_wav_size();
    if (wav_offset > size) return ESP_ERR_INVALID_ARG;
    const size_t length = capacity < size - wav_offset ? capacity : size - wav_offset;
    if (length == 0U) return ESP_OK;

    uint8_t *output = buffer;
    size_t header_part = 0;
    if (wav_offset < RECORDING_WAV_HEADER_BYTES) {
        uint8_t header[RECORDING_WAV_HEADER_BYTES];
        wav_header(header);
        header_part = RECORDING_WAV_HEADER_BYTES - wav_offset;
        if (header_part > length) header_part = length;
        memcpy(output, header + wav_offset, header_part);
    }
    if (length > header_part) {
        const size_t pcm_offset = wav_offset + header_part - RECORDING_WAV_HEADER_BYTES;
        esp_err_t error = esp_partition_read(partition, pcm_offset, output + header_part, length - header_part);
        if (error != ESP_OK) {
            memset(output, 0, length);
            state = CLIP_BLOCKED;
            return error;
        }
    }
    *count = length;
    return ESP_OK;
}
