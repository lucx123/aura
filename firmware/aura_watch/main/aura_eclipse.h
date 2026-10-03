#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "aura_storage_format.h"
#include "aura_evidence_store.h"

typedef struct {
    bool mounted;
    uint64_t capacity_bytes;
    esp_err_t last_error;
    esp_err_t probe_error;
    aura_storage_format_t detected_format;
    uint32_t volume_lba;
    uint64_t volume_sectors;
    uint16_t exfat_version;
    uint8_t exfat_sector_shift;
    uint8_t exfat_fat_count;
    uint32_t records_written;
    bool last_record_verified;
} aura_eclipse_storage_t;

void aura_eclipse_init(void);

esp_err_t aura_eclipse_storage_mount(void);
esp_err_t aura_eclipse_storage_unmount(void);
void aura_eclipse_storage_status(aura_eclipse_storage_t *out);
esp_err_t aura_eclipse_log(const char *event, const char *detail);
esp_err_t aura_eclipse_log_internal(const char *event, const char *detail);
void aura_eclipse_evidence_status(aura_evidence_status_t *out);
esp_err_t aura_eclipse_evidence_read(unsigned index, char *line, size_t capacity);
esp_err_t aura_eclipse_evidence_copy(char lines[AURA_EVIDENCE_CAPACITY][AURA_EVIDENCE_LINE_MAX], unsigned *count);
