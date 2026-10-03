#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AURA_EVIDENCE_CAPACITY 8
#define AURA_EVIDENCE_LINE_MAX 192

typedef struct {
    uint32_t sequence;
    uint8_t count;
    bool verified;
    esp_err_t error;
} aura_evidence_status_t;

/* Caller serializes access. The namespace is separate from preferences.
 * No partition erase or recovery format is performed on errors. */
esp_err_t aura_evidence_store_status(aura_evidence_status_t *out);
esp_err_t aura_evidence_store_append(const char *line, aura_evidence_status_t *out);
/* index 0 is the oldest retained record. */
esp_err_t aura_evidence_store_read(unsigned index, char *line, size_t capacity);
/* One coherent read for USB export, even if a later append changes the ring. */
esp_err_t aura_evidence_store_copy(char lines[AURA_EVIDENCE_CAPACITY][AURA_EVIDENCE_LINE_MAX], unsigned *count);
