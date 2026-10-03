/* Synthetic file-backed disk for testing the vendored production FatFs core.
 * Every sector operation is bounded to the temporary image supplied by the test.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"

#define SECTOR_SIZE 512u
#define SECTOR_COUNT 131072u /* 64 MiB: enough clusters for FAT32 with 512-byte AU. */
#define EXISTING_PATH "0:/USER/existing-user-data.bin"
#define EVENTS_PATH "0:/AURA/eclipse/events.jsonl"

static const char old_record[] = "{\"time\":1780000000,\"event\":\"existing\",\"detail\":\"kept\"}\n";
static const char new_record[] = "{\"time\":1780000001,\"event\":\"diagnostic\",\"detail\":\"synthetic\"}\n";
static FILE *image;
static unsigned reads, writes;
static FATFS fs;
PARTITION VolToPart[FF_VOLUMES] = {{0, 0}, {0, 0}};

static int valid_range(LBA_t sector, UINT count)
{
    return count && sector < SECTOR_COUNT && count <= SECTOR_COUNT - sector;
}

DSTATUS disk_initialize(BYTE drive) { return drive == 0 && image ? 0 : STA_NOINIT; }
DSTATUS disk_status(BYTE drive) { return disk_initialize(drive); }

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive || !image || !buffer || !valid_range(sector, count)) return RES_PARERR;
    reads++;
    if (fseek(image, (long)sector * SECTOR_SIZE, SEEK_SET)) return RES_ERROR;
    return fread(buffer, SECTOR_SIZE, count, image) == count ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive || !image || !buffer || !valid_range(sector, count)) return RES_PARERR;
    writes++;
    if (fseek(image, (long)sector * SECTOR_SIZE, SEEK_SET)) return RES_ERROR;
    return fwrite(buffer, SECTOR_SIZE, count, image) == count ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (drive || !image) return RES_PARERR;
    switch (command) {
    case CTRL_SYNC: return fflush(image) ? RES_ERROR : RES_OK;
    case GET_SECTOR_COUNT:
        if (!buffer) return RES_PARERR;
        *(LBA_t *)buffer = SECTOR_COUNT;
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (!buffer) return RES_PARERR;
        *(WORD *)buffer = SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (!buffer) return RES_PARERR;
        *(DWORD *)buffer = 1;
        return RES_OK;
    default: return RES_PARERR; /* Never discard, erase or trim an image. */
    }
}

DWORD get_fattime(void)
{
    return ((2026u - 1980u) << 25) | (10u << 21) | (3u << 16) | (12u << 11);
}

static void require(FRESULT result, const char *operation)
{
    if (result != FR_OK) {
        fprintf(stderr, "%s: FRESULT=%d\n", operation, result);
        exit(2);
    }
}

static void directory(const char *path)
{
    FRESULT result = f_mkdir(path);
    if (result != FR_EXIST) require(result, path);
}

static void write_exact(const char *path, const void *data, UINT length, BYTE flags)
{
    FIL file = {0};
    UINT written = 0;
    require(f_open(&file, path, flags), "open");
    require(f_write(&file, data, length, &written), "write");
    if (written != length) {
        fprintf(stderr, "short write: %u of %u bytes\n", written, length);
        exit(2);
    }
    require(f_close(&file), "close");
}

static void summary(FRESULT result)
{
    printf("{\"result\":%d,\"filesystem\":%u,\"exfat\":%d,"
           "\"fsize_bytes\":%u,\"lba_bytes\":%u,\"fatfs_bytes\":%u,"
           "\"fil_bytes\":%u,\"reads\":%u,\"writes\":%u}\n",
           result, fs.fs_type, FF_FS_EXFAT, (unsigned)sizeof(FSIZE_t),
           (unsigned)sizeof(LBA_t), (unsigned)sizeof(FATFS), (unsigned)sizeof(FIL),
           reads, writes);
}

int main(int argc, char **argv)
{
    if (argc < 3) return 1;
    const char *operation = argv[1];
    int formatting = !strcmp(operation, "format");
    image = fopen(argv[2], formatting ? "w+b" : "r+b");
    if (!image) { perror("temporary image"); return 1; }

    if (formatting) {
        if (argc != 5) return 1;
        if (fseek(image, (long)SECTOR_COUNT * SECTOR_SIZE - 1, SEEK_SET) ||
            fputc(0, image) == EOF || fflush(image)) return 1;
        BYTE work[4096];
        int exfat = !strcmp(argv[3], "EXFAT");
        int sfd = !strcmp(argv[4], "SFD");
        if (!exfat && strcmp(argv[3], "FAT32")) return 1;
        if (!sfd && strcmp(argv[4], "MBR")) return 1;
        if (!sfd) {
            LBA_t partition_sizes[] = {100, 0, 0, 0};
            require(f_fdisk(0, partition_sizes, work), "partition synthetic disk");
        }
        MKFS_PARM options = {(BYTE)((exfat ? FM_EXFAT : FM_FAT32) | (sfd ? FM_SFD : 0)),
                             1, 0, 0, 512};
        FRESULT result = f_mkfs("0:", &options, work, sizeof(work));
        summary(result);
    } else if (!strcmp(operation, "bounds")) {
        BYTE buffer[SECTOR_SIZE] = {0};
        if (disk_read(0, buffer, SECTOR_COUNT, 1) != RES_PARERR ||
            disk_read(0, buffer, SECTOR_COUNT - 1, 2) != RES_PARERR ||
            disk_read(0, buffer, UINT32_MAX, UINT32_MAX) != RES_PARERR ||
            disk_write(0, buffer, SECTOR_COUNT, 1) != RES_PARERR ||
            disk_read(0, buffer, 0, 0) != RES_PARERR ||
            disk_read(1, buffer, 0, 1) != RES_PARERR || reads || writes) return 2;
        summary(FR_OK);
    } else {
        FRESULT mounted = f_mount(&fs, "0:", 1);
        if (!strcmp(operation, "mount")) {
            summary(mounted);
        } else {
            require(mounted, "mount synthetic disk");
            if (!strcmp(operation, "seed")) {
                BYTE existing[4097];
                for (unsigned i = 0; i < sizeof(existing); i++) existing[i] = (BYTE)(i * 37u + 11u);
                directory("0:/USER");
                write_exact(EXISTING_PATH, existing, sizeof(existing), FA_CREATE_NEW | FA_WRITE);
                directory("0:/AURA");
                directory("0:/AURA/eclipse");
                write_exact(EVENTS_PATH, old_record, sizeof(old_record) - 1, FA_CREATE_NEW | FA_WRITE);
            } else if (!strcmp(operation, "append")) {
                directory("0:/AURA");
                directory("0:/AURA/eclipse");
                directory("0:/AURA/eclipse/sessions");
                write_exact(EVENTS_PATH, new_record, sizeof(new_record) - 1, FA_OPEN_APPEND | FA_WRITE);
            } else if (!strcmp(operation, "export")) {
                if (argc != 5) return 1;
                FILE *output = fopen(argv[4], "wb");
                if (!output) return 1;
                FIL file = {0};
                BYTE buffer[512];
                UINT count;
                require(f_open(&file, argv[3], FA_READ), "open for export");
                do {
                    require(f_read(&file, buffer, sizeof(buffer), &count), "read for export");
                    if (fwrite(buffer, 1, count, output) != count) return 2;
                } while (count);
                require(f_close(&file), "close after export");
                if (fclose(output)) return 2;
            } else return 1;
            summary(FR_OK);
        }
        require(f_mount(NULL, "0:", 0), "unmount synthetic disk");
    }
    if (fclose(image)) return 2;
    return 0;
}
