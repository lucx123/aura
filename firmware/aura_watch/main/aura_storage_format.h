#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AURA_STORAGE_FORMAT_UNKNOWN = 0,
    AURA_STORAGE_FORMAT_FAT12,
    AURA_STORAGE_FORMAT_FAT16,
    AURA_STORAGE_FORMAT_FAT32,
    AURA_STORAGE_FORMAT_EXFAT,
    AURA_STORAGE_FORMAT_NTFS,
    AURA_STORAGE_FORMAT_GPT,
} aura_storage_format_t;

/* Examines only the first 512 bytes of a complete sector. The result identifies
 * a plausible volume boot header or GPT layout, not filesystem integrity,
 * mountability, or data safety. No I/O, allocation, SDK or file access.
 * GPT means a primary header (LBA 1) or a pure protective MBR clue; no CRC check. */
aura_storage_format_t aura_storage_format_detect(const uint8_t *sector, size_t size);
const char *aura_storage_format_name(aura_storage_format_t format);

/* Extract an ordinary primary MBR partition's first sector (index 0..3), only
 * when its entire extent fits the actual card capacity. Rejects protective GPT
 * entries. On failure clears a non-NULL out_lba, preventing stale sector reads.
 * The caller owns all disk reads and must supply the buffer's actual size. */
bool aura_storage_partition_get(const uint8_t *sector, size_t size, unsigned index,
                                uint64_t total_sectors, uint32_t *out_lba);
