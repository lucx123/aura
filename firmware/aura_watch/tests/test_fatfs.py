"""Test the production FatFs core using temporary images, without physical storage.

python tests/test_fatfs.py --cc path/to/zig.exe

Both builds use a copy of the vendored ffconf.h. Only FF_FS_EXFAT changes in that
fixture; the firmware and installed ESP-IDF are never edited by the tests.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'components/fatfs'
SDK_CONFIG = '''#pragma once
#define CONFIG_FATFS_VOLUME_COUNT 2
#define CONFIG_FATFS_LFN_HEAP 1
#define CONFIG_FATFS_CODEPAGE 437
#define CONFIG_FATFS_MAX_LFN 255
#define CONFIG_FATFS_API_ENCODING_ANSI_OEM 1
#define CONFIG_FATFS_FS_LOCK 0
#define CONFIG_FATFS_TIMEOUT_MS 10000
#define CONFIG_FATFS_PER_FILE_CACHE 1
#define CONFIG_FATFS_USE_STRFUNC_NONE 1
#define CONFIG_FATFS_USE_FASTSEEK 0
#define CONFIG_FATFS_USE_LABEL 1
#define CONFIG_FATFS_DONT_TRUST_FREE_CLUSTER_CNT 0
#define CONFIG_FATFS_DONT_TRUST_LAST_ALLOC 0
#define CONFIG_FATFS_USE_DYN_BUFFERS 0
#define CONFIG_WL_SECTOR_SIZE 4096
'''


def build_fixture(cc, directory, exfat):
    fixture = directory / f'core-{exfat}'
    fixture.mkdir()
    for name in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h'):
        shutil.copyfile(VENDOR / 'src' / name, fixture / name)
    configuration = (fixture / 'ffconf.h').read_bytes()
    configuration, changed = re.subn(rb'(#define\s+FF_FS_EXFAT\s+)[01](?=\s)',
                                     lambda match: match[1] + str(exfat).encode(), configuration)
    if changed != 1:
        raise ValueError('Expected one FF_FS_EXFAT definition')
    (fixture / 'ffconf.h').write_bytes(configuration)
    (fixture / 'sdkconfig.h').write_text(SDK_CONFIG)
    freertos = fixture / 'freertos'
    freertos.mkdir()
    (freertos / 'FreeRTOS.h').write_text('#pragma once\n#define portTICK_PERIOD_MS 1\n')
    (freertos / 'semphr.h').write_text('#pragma once\n')
    system = fixture / 'sys'
    system.mkdir()
    (system / 'param.h').write_text('''#pragma once
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
''')
    executable = fixture / 'fatfs_harness.exe'
    command = [cc]
    if pathlib.Path(cc).name.lower().startswith('zig'):
        command.append('cc')
    command += ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-parameter', '-I', str(fixture), '-o', str(executable),
                str(ROOT / 'tests/fatfs_harness.c'), str(fixture / 'ff.c'),
                str(fixture / 'ffunicode.c'), str(VENDOR / 'port/linux/ffsystem.c')]
    environment = dict(os.environ)
    environment['ZIG_GLOBAL_CACHE_DIR'] = str(pathlib.Path(tempfile.gettempdir()) / 'aura-fatfs-zig-cache')
    environment['ZIG_LOCAL_CACHE_DIR'] = str(directory / 'zig-local-cache')
    subprocess.run(command, check=True, timeout=120, env=environment)
    return executable


class FatFsTests(unittest.TestCase):
    def run_core(self, flag, command, image, *extra):
        process = subprocess.run([str(self.cores[flag]), command, str(image), *extra],
                                 check=True, capture_output=True, text=True, timeout=30)
        result = json.loads(process.stdout)
        self.assertEqual(result['exfat'], flag)
        self.assertEqual(result['lba_bytes'], 4)
        return result

    def format_image(self, flag, filesystem, layout):
        image = self.directory / f'{self.id().split(".")[-1]}-{flag}-{filesystem}-{layout}.img'
        self.assertEqual(self.run_core(flag, 'format', image, filesystem, layout)['result'], 0)
        return image

    def export(self, flag, image, filename):
        output = self.directory / 'export.bin'
        status = self.run_core(flag, 'export', image, filename, str(output))
        self.assertEqual(status['result'], 0)
        self.assertEqual(status['writes'], 0)
        return output.read_bytes()

    def exercise(self, image, writer):
        self.assertEqual(self.run_core(writer, 'seed', image)['result'], 0)
        before = self.export(writer, image, '0:/USER/existing-user-data.bin')
        self.assertEqual(before, bytes((i * 37 + 11) & 255 for i in range(4097)))
        old = self.export(writer, image, '0:/AURA/eclipse/events.jsonl')
        self.assertEqual(json.loads(old), {'time': 1780000000, 'event': 'existing', 'detail': 'kept'})
        for _ in range(3):
            self.assertEqual(self.run_core(writer, 'append', image)['result'], 0)
        self.assertEqual(self.export(writer, image, '0:/USER/existing-user-data.bin'), before)
        events = self.export(writer, image, '0:/AURA/eclipse/events.jsonl')
        self.assertTrue(events.startswith(old))
        records = [json.loads(line) for line in events.splitlines()]
        self.assertEqual(len(records), 4)
        self.assertEqual(records[0], json.loads(old))
        self.assertEqual(records[1:], [{'time': 1780000001, 'event': 'diagnostic',
                                       'detail': 'synthetic'}] * 3)

    def test_upstream_integrity_and_configuration(self):
        manifest = json.loads((VENDOR / 'upstream-sha256.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['upstream_tag'], 'v5.5.5')
        self.assertEqual(manifest['upstream_commit'], 'b774170ff46c393eeb5e495ea37936038d3f4f4f')
        self.assertEqual(len(manifest['files']), 103)
        for name, expected in manifest['files'].items():
            content = (VENDOR / name).read_bytes()
            if name == 'src/ffconf.h':
                self.assertRegex(content, rb'#define\s+FF_LBA64\s+0\b')
                self.assertRegex(content, rb'#define\s+FF_FS_EXFAT\s+[01]\b')
                # The only permitted source patch is the exFAT flag. The original
                # hash is immutable; normalize line endings for header editing.
                content = re.sub(rb'(#define\s+FF_FS_EXFAT\s+)[01](?=\s)',
                                 lambda match: match[1] + b'0', content)
                if hashlib.sha256(content).hexdigest() != expected:
                    content = content.replace(b'\r\n', b'\n')
                    self.assertEqual(hashlib.sha256(content).hexdigest(),
                                     manifest['ffconf_normalized_lf_sha256'])
                    continue
            self.assertEqual(hashlib.sha256(content).hexdigest(), expected, name)

    def test_fat32_mbr_roundtrip_in_both_builds(self):
        for flag in (0, 1):
            with self.subTest(exfat=flag):
                image = self.format_image(flag, 'FAT32', 'MBR')
                self.exercise(image, flag)
                other = 1 - flag
                self.assertEqual(self.run_core(other, 'mount', image)['filesystem'], 3)
                self.assertEqual(self.export(other, image, '0:/USER/existing-user-data.bin'),
                                 self.export(flag, image, '0:/USER/existing-user-data.bin'))

    def test_fat32_superfloppy_roundtrip(self):
        image = self.format_image(1, 'FAT32', 'SFD')
        self.exercise(image, 0)
        self.assertEqual(self.run_core(1, 'mount', image)['filesystem'], 3)

    def test_exfat_mbr_roundtrip_and_preserves_existing_files(self):
        image = self.format_image(1, 'EXFAT', 'MBR')
        with image.open('rb') as file:
            mbr = file.read(512)
            self.assertEqual(mbr[510:512], b'\x55\xaa')
            partition_lba = int.from_bytes(mbr[454:458], 'little')
            self.assertGreater(partition_lba, 0)
            file.seek(partition_lba * 512)
            boot = file.read(512)
        self.assertEqual(boot[3:11], b'EXFAT   ')
        self.assertEqual(boot[108], 9)
        self.assertEqual(boot[110], 1)
        self.exercise(image, 1)
        self.assertEqual(self.run_core(1, 'mount', image)['filesystem'], 4)

    def test_exfat_superfloppy_roundtrip(self):
        image = self.format_image(1, 'EXFAT', 'SFD')
        self.exercise(image, 1)
        self.assertEqual(self.run_core(1, 'mount', image)['filesystem'], 4)

    def test_disabled_exfat_rejects_without_writing_or_formatting(self):
        image = self.format_image(1, 'EXFAT', 'MBR')
        self.exercise(image, 1)
        before = hashlib.sha256(image.read_bytes()).hexdigest()
        result = self.run_core(0, 'mount', image)
        self.assertEqual(result['result'], 13)  # FR_NO_FILESYSTEM
        self.assertGreater(result['reads'], 0)
        self.assertEqual(result['writes'], 0)
        self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), before)
        self.assertEqual(self.run_core(1, 'mount', image)['result'], 0)

    def test_invalid_image_returns_no_filesystem_without_writes(self):
        image = self.directory / 'blank.img'
        with image.open('wb') as file:
            file.truncate(64 * 1024 * 1024)
        for flag in (0, 1):
            result = self.run_core(flag, 'mount', image)
            self.assertEqual(result['result'], 13)
            self.assertEqual(result['writes'], 0)

    def test_disk_bounds_are_checked_without_io(self):
        image = self.format_image(1, 'FAT32', 'MBR')
        for flag in (0, 1):
            result = self.run_core(flag, 'bounds', image)
            self.assertEqual(result['result'], 0)
            self.assertEqual(result['reads'], 0)
            self.assertEqual(result['writes'], 0)

    def test_file_abi_changes_are_shared_by_each_build(self):
        image = self.format_image(1, 'FAT32', 'MBR')
        disabled = self.run_core(0, 'mount', image)
        enabled = self.run_core(1, 'mount', image)
        self.assertEqual(disabled['fsize_bytes'], 4)
        self.assertEqual(enabled['fsize_bytes'], 8)
        self.assertGreater(enabled['fatfs_bytes'], disabled['fatfs_bytes'])
        self.assertGreater(enabled['fil_bytes'], disabled['fil_bytes'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', required=True, help='Zig executable, or native C compiler')
    arguments, remaining = parser.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='aura-fatfs-tests-') as temporary:
        FatFsTests.directory = pathlib.Path(temporary)
        FatFsTests.cores = {flag: build_fixture(arguments.cc, FatFsTests.directory, flag)
                           for flag in (0, 1)}
        tests = unittest.main(argv=['test_fatfs.py', *remaining], exit=False)
        raise SystemExit(not tests.result.wasSuccessful())
