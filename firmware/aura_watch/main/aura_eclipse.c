#include "aura_eclipse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "aura_eclipse";
static aura_eclipse_storage_t storage;
static aura_evidence_status_t evidence;
static SemaphoreHandle_t storage_lock;
static portMUX_TYPE storage_state_lock = portMUX_INITIALIZER_UNLOCKED;

void aura_eclipse_init(void)
{
    storage_lock = xSemaphoreCreateMutex();
    configASSERT(storage_lock);
    storage.last_error = ESP_ERR_NOT_FOUND;
    storage.probe_error = ESP_ERR_NOT_FOUND;
    aura_evidence_store_status(&evidence);
}

static uint64_t le64(const uint8_t *bytes)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}

/* Read boot metadata only. No filesystem is mounted or written by this probe. */
static esp_err_t probe_card(sdmmc_card_t *card, aura_eclipse_storage_t *out)
{
    out->capacity_bytes = (uint64_t)card->csd.capacity * card->csd.sector_size;
    if (card->csd.sector_size < 512 || card->csd.sector_size > 4096)
        return ESP_ERR_NOT_SUPPORTED;
    uint8_t *sector = heap_caps_malloc(card->csd.sector_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!sector) return ESP_ERR_NO_MEM;
    esp_err_t error = sdmmc_read_sectors(card, sector, 0, 1);
    if (error != ESP_OK) { free(sector); return error; }
    out->detected_format = aura_storage_format_detect(sector, card->csd.sector_size);
    uint32_t partitions[4] = {0};
    for (unsigned i = 0; i < 4; ++i)
        aura_storage_partition_get(sector, card->csd.sector_size, i, card->csd.capacity, &partitions[i]);
    if (out->detected_format == AURA_STORAGE_FORMAT_GPT && card->csd.capacity > 1) {
        error = sdmmc_read_sectors(card, sector, 1, 1);
        if (error == ESP_OK) out->detected_format = aura_storage_format_detect(sector, card->csd.sector_size);
    } else if (out->detected_format == AURA_STORAGE_FORMAT_UNKNOWN) {
        for (unsigned i = 0; i < 4; ++i) {
            if (!partitions[i]) continue;
            error = sdmmc_read_sectors(card, sector, partitions[i], 1);
            if (error != ESP_OK) break;
            out->detected_format = aura_storage_format_detect(sector, card->csd.sector_size);
            if (out->detected_format != AURA_STORAGE_FORMAT_UNKNOWN) {
                out->volume_lba = partitions[i];
                break;
            }
        }
    }
    if (error == ESP_OK && out->detected_format == AURA_STORAGE_FORMAT_EXFAT) {
        out->volume_sectors = le64(sector + 72);
        out->exfat_version = (uint16_t)sector[104] | (uint16_t)sector[105] << 8;
        out->exfat_sector_shift = sector[108];
        out->exfat_fat_count = sector[110];
    }
    free(sector);
    return error;
}

static esp_err_t probe_unmounted_card(aura_eclipse_storage_t *out)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = BSP_SD_CLK; slot.cmd = BSP_SD_CMD; slot.d0 = BSP_SD_D0;
    slot.d1 = slot.d2 = slot.d3 = slot.d4 = slot.d5 = slot.d6 = slot.d7 = GPIO_NUM_NC;
    slot.width = 1;
    esp_err_t error = host.init();
    if (error != ESP_OK) return error;
    bool slot_open = false;
    sdmmc_card_t card = {0};
    error = sdmmc_host_init_slot(host.slot, &slot);
    if (error == ESP_OK) {
        slot_open = true;
        error = sdmmc_card_init(&host, &card);
        if (error == ESP_OK) error = probe_card(&card, out);
    }
    esp_err_t closed = slot_open ? host.deinit_p(host.slot) : sdmmc_host_deinit();
    if (error == ESP_OK) error = closed;
    return error;
}

static esp_err_t ensure_directory(const char *path)
{
    if (mkdir(path, 0755) == 0) return ESP_OK;
    struct stat info;
    if (errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode)) return ESP_OK;
    return ESP_FAIL;
}

esp_err_t aura_eclipse_storage_mount(void)
{
    if (!storage_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    aura_eclipse_storage_t next;
    aura_eclipse_storage_status(&next);
    esp_err_t error = next.mounted ? ESP_OK : bsp_sdcard_mount();
    if (!next.mounted) {
        next.detected_format = AURA_STORAGE_FORMAT_UNKNOWN;
        next.capacity_bytes = next.volume_sectors = 0;
        next.volume_lba = 0;
        next.exfat_version = next.exfat_sector_shift = next.exfat_fat_count = 0;
        next.probe_error = error == ESP_OK && bsp_sdcard ? probe_card(bsp_sdcard, &next) :
                           probe_unmounted_card(&next);
        ESP_LOGI(TAG, "SD boot: type=%s error=%s bytes=%llu lba=%lu version=0x%04x sector_shift=%u fats=%u",
                 aura_storage_format_name(next.detected_format), esp_err_to_name(next.probe_error),
                 (unsigned long long)next.capacity_bytes, (unsigned long)next.volume_lba,
                 next.exfat_version, next.exfat_sector_shift, next.exfat_fat_count);
    }
    if (error == ESP_OK) {
        next.mounted = true;
        if (bsp_sdcard) next.capacity_bytes = (uint64_t)bsp_sdcard->csd.capacity * bsp_sdcard->csd.sector_size;
        error = ensure_directory(BSP_SD_MOUNT_POINT "/AURA");
        if (error == ESP_OK) error = ensure_directory(BSP_SD_MOUNT_POINT "/AURA/eclipse");
        if (error == ESP_OK) error = ensure_directory(BSP_SD_MOUNT_POINT "/AURA/eclipse/sessions");
    }
    next.last_error = error;
    portENTER_CRITICAL(&storage_state_lock);
    storage = next;
    portEXIT_CRITICAL(&storage_state_lock);
    xSemaphoreGive(storage_lock);
    ESP_LOGI(TAG, "SD prepare: %s", esp_err_to_name(error));
    return error;
}

void aura_eclipse_storage_status(aura_eclipse_storage_t *out)
{
    if (!out) return;
    // Never wait for an SD transaction from the UI or USB status command.
    portENTER_CRITICAL(&storage_state_lock);
    *out = storage;
    portEXIT_CRITICAL(&storage_state_lock);
}

esp_err_t aura_eclipse_storage_unmount(void)
{
    if (!storage_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    aura_eclipse_storage_t current;
    aura_eclipse_storage_status(&current);
    esp_err_t error = current.mounted ? bsp_sdcard_unmount() : ESP_OK;
    if (current.mounted) {
        portENTER_CRITICAL(&storage_state_lock);
        if (error == ESP_OK) storage.mounted = false;
        else storage.last_error = error;
        portEXIT_CRITICAL(&storage_state_lock);
        if (error == ESP_OK) bsp_sdcard = NULL;
    }
    xSemaphoreGive(storage_lock);
    return error;
}

static char *record_line(const char *event, const char *detail)
{
    cJSON *record = cJSON_CreateObject();
    if (!record) return NULL;
    bool valid = cJSON_AddNumberToObject(record, "time", (double)time(NULL)) &&
                 cJSON_AddStringToObject(record, "event", event) &&
                 cJSON_AddStringToObject(record, "detail", detail ? detail : "");
    char *line = valid ? cJSON_PrintUnformatted(record) : NULL;
    cJSON_Delete(record);
    return line;
}

esp_err_t aura_eclipse_log(const char *event, const char *detail)
{
    if (!event || !event[0]) return ESP_ERR_INVALID_ARG;
    aura_eclipse_storage_t current;
    aura_eclipse_storage_status(&current);
    if (!current.mounted || !storage_lock) return ESP_ERR_INVALID_STATE;
    char *line = record_line(event, detail);
    if (!line) return ESP_ERR_NO_MEM;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    FILE *file = fopen(BSP_SD_MOUNT_POINT "/AURA/eclipse/events.jsonl", "a");
    esp_err_t error = ESP_FAIL;
    if (file) {
        int written = fprintf(file, "%s\n", line);
        int closed = fclose(file);
        size_t length = strlen(line);
        if (written > 0 && (size_t)written == length + 1 && closed == 0) {
            /* Confirm the newly appended record by reading only its tail back.
             * Never enumerate or transmit other files from the user's card. */
            FILE *verify = fopen(BSP_SD_MOUNT_POINT "/AURA/eclipse/events.jsonl", "r");
            if (verify) {
                bool matches = fseek(verify, -(long)(length + 1), SEEK_END) == 0;
                for (size_t i = 0; matches && i < length; ++i)
                    matches = fgetc(verify) == (unsigned char)line[i];
                if (matches) matches = fgetc(verify) == '\n';
                int verified_close = fclose(verify);
                if (matches && verified_close == 0) error = ESP_OK;
            }
        }
    }
    portENTER_CRITICAL(&storage_state_lock);
    storage.last_error = error;
    storage.last_record_verified = error == ESP_OK;
    if (error == ESP_OK) ++storage.records_written;
    portEXIT_CRITICAL(&storage_state_lock);
    xSemaphoreGive(storage_lock);
    cJSON_free(line);
    return error;
}

esp_err_t aura_eclipse_log_internal(const char *event, const char *detail)
{
    if (!event || !event[0]) return ESP_ERR_INVALID_ARG;
    if (!storage_lock) return ESP_ERR_INVALID_STATE;
    char *line = record_line(event, detail);
    if (!line) return ESP_ERR_NO_MEM;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    aura_evidence_status_t next;
    esp_err_t error = aura_evidence_store_append(line, &next);
    portENTER_CRITICAL(&storage_state_lock);
    evidence = next;
    portEXIT_CRITICAL(&storage_state_lock);
    xSemaphoreGive(storage_lock);
    cJSON_free(line);
    return error;
}

void aura_eclipse_evidence_status(aura_evidence_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&storage_state_lock);
    *out = evidence;
    portEXIT_CRITICAL(&storage_state_lock);
}

esp_err_t aura_eclipse_evidence_read(unsigned index, char *line, size_t capacity)
{
    if (!storage_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    esp_err_t error = aura_evidence_store_read(index, line, capacity);
    xSemaphoreGive(storage_lock);
    return error;
}

esp_err_t aura_eclipse_evidence_copy(char lines[AURA_EVIDENCE_CAPACITY][AURA_EVIDENCE_LINE_MAX], unsigned *count)
{
    if (!storage_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    esp_err_t error = aura_evidence_store_copy(lines, count);
    xSemaphoreGive(storage_lock);
    return error;
}
