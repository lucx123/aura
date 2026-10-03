# FatFs supplied by ESP-IDF 5.5.5

This directory is the complete `components/fatfs` component from the official
[ESP-IDF v5.5.5 tag](https://github.com/espressif/esp-idf/tree/v5.5.5/components/fatfs).
The source commit is `b774170ff46c393eeb5e495ea37936038d3f4f4f`.
FatFs identifies itself as R0.15 with patch 2 in `src/ff.c`.

The source checkout's tag, commit and clean component status were checked before
copying. All 103 upstream files were copied without modifying their bytes.
`upstream-sha256.json` records their original SHA-256 hashes. These hashes describe
the clean checkout's bytes, including its line endings. `.gitattributes` preserves
those bytes in this vendored directory.

The original copyright and license notices remain in the source files. FatFs's
redistribution notice is in `src/ff.c` and `src/ff.h`. Espressif's Apache 2.0 license
is also included as `LICENSE-ESP-IDF.txt` from the same SDK checkout.

## Local changes

The only upstream change is `FF_FS_EXFAT=1` in `src/ffconf.h`; `FF_LBA64=0`
remains unchanged. This adds FAT/exFAT compatibility for ordinary MBR and
superfloppy volumes. Long filenames remain enabled with `CONFIG_FATFS_LFN_HEAP=y`.
`CONFIG_FATFS_USE_LABEL=y` keeps the SDK's label macro defined in the exFAT
runtime expression in `ff.c` (a disabled Kconfig boolean is otherwise undefined).
The provenance, original hash manifest, license copy and Git attributes are
project metadata. The manifest still describes the original source.

On 2026-10-03, a read-only boot-sector probe identified the connected 32 GB card
as NTFS at LBA 2048. FatFs does not support NTFS, including with exFAT enabled.
The firmware leaves this card's format intact and uses its bounded internal
Evidence journal when SD recording fails. exFAT is verified with synthetic host
images; it has not been tested on a physical exFAT card in this session.

## Build and ABI

ESP-IDF gives the project's `components/fatfs` precedence over the SDK component.
Run `idf.py reconfigure` or use a fresh build directory after adding this override.
Check that the configured component and all FatFs include paths point here.

`ff.h` includes the `ffconf.h` beside it. This complete component therefore shares
one configuration across the FatFs core, disk drivers, VFS, BSP and other users.
Enabling exFAT changes `FSIZE_t` and the layouts of filesystem and file objects;
rebuild all consumers together. A compiler definition or a different include path
alone cannot safely override the original header.

Keep `FF_LBA64=0`: this SDK's disk-driver interface still passes 32-bit sector
numbers. Enabling 64-bit LBA and GPT also requires changes to the disk interface,
not just changing the FatFs macro. Card formatting remains disabled in the BSP.

## Synthetic tests

From the firmware directory, run:

```powershell
python tests/test_fatfs.py --cc path/to/zig.exe
```

The tests compile copied production sources with both exFAT settings and use only
temporary file images. They never access a board, serial port or physical card.
The temporary fixtures exercise both values of `FF_FS_EXFAT`; the installed SDK
and vendored production header are not modified by the tests.
