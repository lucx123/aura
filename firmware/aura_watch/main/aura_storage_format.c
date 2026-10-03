#include "aura_storage_format.h"

#include <string.h>

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t le64(const uint8_t *p)
{
    return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static bool power_two(uint32_t value) { return value && !(value & (value - 1)); }

static bool sector_bytes(uint16_t value)
{
    return value == 512 || value == 1024 || value == 2048 || value == 4096;
}

static bool zero_bytes(const uint8_t *p, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) if (p[i]) return false;
    return true;
}

static bool boot_signature(const uint8_t *sector)
{
    return sector[510] == 0x55 && sector[511] == 0xaa;
}

static bool jump_instruction(const uint8_t *sector)
{
    return sector[0] == 0xe9 || (sector[0] == 0xeb && sector[2] == 0x90);
}

static bool gpt_header(const uint8_t *sector)
{
    if (memcmp(sector, "EFI PART", 8) || le32(sector + 8) != 0x00010000 ||
        le32(sector + 12) < 92 || le32(sector + 12) > 512 || le32(sector + 20)) return false;
    uint64_t current = le64(sector + 24), backup = le64(sector + 32);
    uint64_t first = le64(sector + 40), last = le64(sector + 48);
    uint64_t entries = le64(sector + 72);
    uint32_t entry_count = le32(sector + 80), entry_size = le32(sector + 84);
    return current == 1 && backup > 1 && first > 2 && last >= first && last < backup &&
           entries > 1 && entries < first && entry_count && entry_size >= 128 && power_two(entry_size);
}

static bool protective_mbr(const uint8_t *sector)
{
    unsigned protective_count = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const uint8_t *entry = sector + 446 + i * 16;
        if (zero_bytes(entry, 16)) continue;
        if (entry[0] || entry[4] != 0xee || le32(entry + 8) != 1 || !le32(entry + 12)) return false;
        ++protective_count;
    }
    return protective_count == 1;
}

static bool exfat_header(const uint8_t *sector)
{
    if (memcmp(sector + 3, "EXFAT   ", 8) || sector[0] != 0xeb || sector[1] != 0x76 ||
        sector[2] != 0x90 || !zero_bytes(sector + 11, 53)) return false;
    unsigned bytes_shift = sector[108], cluster_shift = sector[109], fats = sector[110];
    if (bytes_shift < 9 || bytes_shift > 12 || cluster_shift > 25 - bytes_shift ||
        (fats != 1 && fats != 2) || le16(sector + 104) != 0x0100) return false;
    uint64_t length = le64(sector + 72);
    uint32_t fat_offset = le32(sector + 80), fat_length = le32(sector + 84);
    uint32_t heap = le32(sector + 88), clusters = le32(sector + 92), root = le32(sector + 96);
    if (length < (UINT64_C(1) << (20 - bytes_shift)) || fat_offset < 24 || !fat_length ||
        !clusters || clusters > UINT32_C(0xfffffff5) || root < 2 || (uint64_t)root > (uint64_t)clusters + 1 ||
        (uint64_t)fat_offset + (uint64_t)fat_length * fats > heap || heap >= length) return false;
    uint64_t heap_sectors = (uint64_t)clusters << cluster_shift;
    uint64_t fat_bytes = (uint64_t)fat_length << bytes_shift;
    return heap_sectors <= length - heap && ((uint64_t)clusters + 2) * 4 <= fat_bytes;
}

static bool ntfs_header(const uint8_t *sector)
{
    if (memcmp(sector + 3, "NTFS    ", 8) || !jump_instruction(sector) ||
        !sector_bytes(le16(sector + 11)) || !zero_bytes(sector + 14, 7) ||
        le16(sector + 22) || le32(sector + 32)) return false;
    /* Positive values encode a power-of-two cluster sector count. Newer NTFS
     * headers can encode larger counts with a negative signed-byte exponent. */
    unsigned spc = sector[13];
    uint64_t sectors_per_cluster;
    if (spc <= 128) {
        if (!power_two(spc)) return false;
        sectors_per_cluster = spc;
    } else {
        unsigned shift = 256 - spc;
        if (shift > 31) return false;
        sectors_per_cluster = UINT64_C(1) << shift;
    }
    uint64_t length = le64(sector + 40), mft = le64(sector + 48), mirror = le64(sector + 56);
    uint64_t clusters = length / sectors_per_cluster;
    return clusters && mft < clusters && mirror < clusters;
}

static aura_storage_format_t fat_header(const uint8_t *sector)
{
    uint16_t bytes = le16(sector + 11), reserved = le16(sector + 14);
    unsigned spc = sector[13], fats = sector[16];
    uint16_t root_entries = le16(sector + 17), total16 = le16(sector + 19), fat16 = le16(sector + 22);
    uint32_t total32 = le32(sector + 32);
    if (!jump_instruction(sector) || !sector_bytes(bytes) || !power_two(spc) || spc > 128 ||
        !reserved || (fats != 1 && fats != 2) || (sector[21] != 0xf0 && sector[21] < 0xf8) ||
        (!total16 && !total32) || (total16 && total32)) return AURA_STORAGE_FORMAT_UNKNOWN;
    uint64_t total = total16 ? total16 : total32;
    uint64_t fat_sectors = fat16 ? fat16 : le32(sector + 36);
    if (!fat_sectors) return AURA_STORAGE_FORMAT_UNKNOWN;
    uint64_t root_sectors = ((uint64_t)root_entries * 32 + bytes - 1) / bytes;
    uint64_t overhead = reserved + fats * fat_sectors + root_sectors;
    if (overhead >= total) return AURA_STORAGE_FORMAT_UNKNOWN;
    uint64_t clusters = (total - overhead) / spc;
    if (!clusters) return AURA_STORAGE_FORMAT_UNKNOWN;
    /* Microsoft FAT Specification section 3.5: type depends on cluster count,
     * never on the informational FAT12/FAT16/FAT32 label in boot code. */
    aura_storage_format_t type = clusters < 4085 ? AURA_STORAGE_FORMAT_FAT12 :
        clusters < 65525 ? AURA_STORAGE_FORMAT_FAT16 : AURA_STORAGE_FORMAT_FAT32;
    uint64_t entries = clusters + 2;
    uint64_t required_bytes = type == AURA_STORAGE_FORMAT_FAT12 ? (entries * 3 + 1) / 2 :
                              entries * (type == AURA_STORAGE_FORMAT_FAT16 ? 2 : 4);
    if (required_bytes > fat_sectors * bytes) return AURA_STORAGE_FORMAT_UNKNOWN;
    if (type == AURA_STORAGE_FORMAT_FAT32) {
        uint32_t root = le32(sector + 44);
        if (fat16 || root_entries || total16 || le16(sector + 42) ||
            clusters >= UINT64_C(0x0ffffff6) || root < 2 || root > clusters + 1)
            return AURA_STORAGE_FORMAT_UNKNOWN;
    } else if (!fat16 || !root_entries) return AURA_STORAGE_FORMAT_UNKNOWN;
    return type;
}

aura_storage_format_t aura_storage_format_detect(const uint8_t *sector, size_t size)
{
    if (!sector || size < 512) return AURA_STORAGE_FORMAT_UNKNOWN;
    if (gpt_header(sector)) return AURA_STORAGE_FORMAT_GPT;
    if (!boot_signature(sector)) return AURA_STORAGE_FORMAT_UNKNOWN;
    if (exfat_header(sector)) return AURA_STORAGE_FORMAT_EXFAT;
    if (ntfs_header(sector)) return AURA_STORAGE_FORMAT_NTFS;
    /* Recognized OEM names with malformed parameters cannot fall back to FAT. */
    if (!memcmp(sector + 3, "EXFAT   ", 8) || !memcmp(sector + 3, "NTFS    ", 8))
        return AURA_STORAGE_FORMAT_UNKNOWN;
    aura_storage_format_t format = fat_header(sector);
    if (format != AURA_STORAGE_FORMAT_UNKNOWN) return format;
    return protective_mbr(sector) ? AURA_STORAGE_FORMAT_GPT : AURA_STORAGE_FORMAT_UNKNOWN;
}

const char *aura_storage_format_name(aura_storage_format_t format)
{
    switch (format) {
    case AURA_STORAGE_FORMAT_FAT12: return "FAT12";
    case AURA_STORAGE_FORMAT_FAT16: return "FAT16";
    case AURA_STORAGE_FORMAT_FAT32: return "FAT32";
    case AURA_STORAGE_FORMAT_EXFAT: return "exFAT";
    case AURA_STORAGE_FORMAT_NTFS: return "NTFS";
    case AURA_STORAGE_FORMAT_GPT: return "GPT";
    default: return "Desconocido";
    }
}

bool aura_storage_partition_get(const uint8_t *sector, size_t size, unsigned index,
                                uint64_t total_sectors, uint32_t *out_lba)
{
    if (!out_lba) return false;
    *out_lba = 0;
    if (!sector || size < 512 || index >= 4 || !total_sectors || !boot_signature(sector)) return false;
    const uint8_t *entry = sector + 446 + index * 16;
    if ((entry[0] != 0 && entry[0] != 0x80) || !entry[4] || entry[4] == 0xee) return false;
    uint32_t start = le32(entry + 8), count = le32(entry + 12);
    if (!start || !count || start >= total_sectors || count > total_sectors - start) return false;
    *out_lba = start;
    return true;
}
