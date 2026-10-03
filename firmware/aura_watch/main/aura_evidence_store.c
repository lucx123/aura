#include "aura_evidence_store.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"

#define JOURNAL_MAGIC 0x41555245u
typedef struct {
    uint32_t magic;
    uint16_t version, count, next, reserved;
    uint32_t sequence;
    char lines[AURA_EVIDENCE_CAPACITY][AURA_EVIDENCE_LINE_MAX];
} journal_t;

static esp_err_t load(nvs_handle_t handle, journal_t *journal)
{
    size_t size = sizeof(*journal);
    esp_err_t error = nvs_get_blob(handle, "journal", journal, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        memset(journal, 0, sizeof(*journal));
        journal->magic = JOURNAL_MAGIC;
        journal->version = 1;
        return ESP_OK;
    }
    if (error != ESP_OK) return error;
    if (size != sizeof(*journal) || journal->magic != JOURNAL_MAGIC ||
        journal->version != 1 || journal->count > AURA_EVIDENCE_CAPACITY ||
        journal->next >= AURA_EVIDENCE_CAPACITY || journal->reserved ||
        journal->sequence < journal->count ||
        (journal->count < AURA_EVIDENCE_CAPACITY && journal->next != journal->count))
        return ESP_ERR_INVALID_STATE;
    for (unsigned i = 0; i < journal->count; ++i)
        if (!journal->lines[i][0] || !memchr(journal->lines[i], 0, AURA_EVIDENCE_LINE_MAX))
            return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

static void describe(const journal_t *journal, aura_evidence_status_t *out)
{
    out->count = (uint8_t)journal->count;
    out->sequence = journal->sequence;
}

esp_err_t aura_evidence_store_status(aura_evidence_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = (aura_evidence_status_t){0};
    journal_t *journal = malloc(sizeof(*journal));
    if (!journal) return out->error = ESP_ERR_NO_MEM;
    nvs_handle_t handle;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "evidence", NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    else if (error == ESP_OK) {
        error = load(handle, journal);
        if (error == ESP_OK) describe(journal, out);
        nvs_close(handle);
    }
    free(journal);
    return out->error = error;
}

esp_err_t aura_evidence_store_append(const char *line, aura_evidence_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = (aura_evidence_status_t){0};
    if (!line || !line[0] || strnlen(line, AURA_EVIDENCE_LINE_MAX) >= AURA_EVIDENCE_LINE_MAX ||
        strchr(line, '\n') || strchr(line, '\r')) return out->error = ESP_ERR_INVALID_ARG;
    journal_t *journal = malloc(sizeof(*journal)), *verify = malloc(sizeof(*verify));
    if (!journal || !verify) {
        free(journal); free(verify);
        return out->error = ESP_ERR_NO_MEM;
    }
    nvs_handle_t handle;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "evidence", NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = load(handle, journal);
        if (error == ESP_OK) {
            describe(journal, out);
            if (journal->sequence == UINT32_MAX) error = ESP_ERR_NOT_SUPPORTED;
            else {
                memset(journal->lines[journal->next], 0, AURA_EVIDENCE_LINE_MAX);
                strcpy(journal->lines[journal->next], line);
                journal->next = (journal->next + 1) % AURA_EVIDENCE_CAPACITY;
                if (journal->count < AURA_EVIDENCE_CAPACITY) ++journal->count;
                ++journal->sequence;
                error = nvs_set_blob(handle, "journal", journal, sizeof(*journal));
                if (error == ESP_OK) error = nvs_commit(handle);
            }
        }
        nvs_close(handle);
    }
    /* Read through a new handle after commit, then compare the entire journal. */
    if (error == ESP_OK) {
        error = nvs_open_from_partition("aura_cfg", "evidence", NVS_READONLY, &handle);
        if (error == ESP_OK) {
            error = load(handle, verify);
            if (error == ESP_OK && memcmp(journal, verify, sizeof(*journal))) error = ESP_FAIL;
            nvs_close(handle);
        }
        if (error == ESP_OK) { describe(verify, out); out->verified = true; }
    }
    free(verify); free(journal);
    return out->error = error;
}

esp_err_t aura_evidence_store_read(unsigned index, char *line, size_t capacity)
{
    if (!line || !capacity) return ESP_ERR_INVALID_ARG;
    line[0] = 0;
    journal_t *journal = malloc(sizeof(*journal));
    if (!journal) return ESP_ERR_NO_MEM;
    nvs_handle_t handle;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "evidence", NVS_READONLY, &handle);
    if (error == ESP_OK) {
        error = load(handle, journal);
        if (error == ESP_OK) {
            if (index >= journal->count) error = ESP_ERR_NOT_FOUND;
            else {
                unsigned slot = ((journal->count == AURA_EVIDENCE_CAPACITY ? journal->next : 0) + index) % AURA_EVIDENCE_CAPACITY;
                size_t length = strlen(journal->lines[slot]);
                if (length >= capacity) error = ESP_ERR_INVALID_SIZE;
                else memcpy(line, journal->lines[slot], length + 1);
            }
        }
        nvs_close(handle);
    }
    free(journal);
    return error;
}

esp_err_t aura_evidence_store_copy(char lines[AURA_EVIDENCE_CAPACITY][AURA_EVIDENCE_LINE_MAX], unsigned *count)
{
    if (!lines || !count) return ESP_ERR_INVALID_ARG;
    *count = 0;
    journal_t *journal = malloc(sizeof(*journal));
    if (!journal) return ESP_ERR_NO_MEM;
    nvs_handle_t handle;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "evidence", NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    else if (error == ESP_OK) {
        error = load(handle, journal);
        if (error == ESP_OK) {
            for (unsigned i = 0; i < journal->count; ++i) {
                unsigned slot = ((journal->count == AURA_EVIDENCE_CAPACITY ? journal->next : 0) + i) % AURA_EVIDENCE_CAPACITY;
                memcpy(lines[i], journal->lines[slot], AURA_EVIDENCE_LINE_MAX);
            }
            *count = journal->count;
        }
        nvs_close(handle);
    }
    free(journal);
    return error;
}
