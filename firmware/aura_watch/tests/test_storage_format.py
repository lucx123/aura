"""Compile and test the production read-only sector classifier on the host.

python tests/test_storage_format.py --cc path/to/zig.exe
Fixtures are synthetic boot headers; no card or user files are accessed.
"""
import argparse
import ctypes
import pathlib
import random
import struct
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
UNKNOWN, FAT12, FAT16, FAT32, EXFAT, NTFS, GPT = range(7)


def put(sector, offset, value, width=4):
    struct.pack_into({2: '<H', 4: '<I', 8: '<Q'}[width], sector, offset, value)


def boot():
    sector = bytearray(512)
    sector[:3] = b'\xeb\x3c\x90'
    sector[510:] = b'\x55\xaa'
    return sector


def fat(clusters, bytes_per_sector=512, sectors_per_cluster=1):
    sector = boot()
    sector[3:11] = b'AURA    '
    put(sector, 11, bytes_per_sector, 2)
    sector[13] = sectors_per_cluster
    reserved, roots = (32, 0) if clusters >= 65525 else (1, 512)
    put(sector, 14, reserved, 2)
    sector[16] = 2
    put(sector, 17, roots, 2)
    sector[21] = 0xf8
    fat_bytes = ((clusters + 2) * 3 + 1) // 2 if clusters < 4085 else (clusters + 2) * (2 if clusters < 65525 else 4)
    fat_sectors = (fat_bytes + bytes_per_sector - 1) // bytes_per_sector
    root_sectors = (roots * 32 + bytes_per_sector - 1) // bytes_per_sector
    total = reserved + 2 * fat_sectors + root_sectors + clusters * sectors_per_cluster
    if total < 65536 and clusters < 65525:
        put(sector, 19, total, 2)
    else:
        put(sector, 32, total)
    if clusters >= 65525:
        put(sector, 36, fat_sectors)
        put(sector, 44, 2)
        sector[82:90] = b'FAT32   '
    else:
        put(sector, 22, fat_sectors, 2)
        sector[54:62] = b'FAT12   ' if clusters < 4085 else b'FAT16   '
    return sector


def exfat():
    sector = boot()
    sector[:3] = b'\xeb\x76\x90'
    sector[3:11] = b'EXFAT   '
    put(sector, 72, 200000, 8)
    put(sector, 80, 24)
    put(sector, 84, 64)
    put(sector, 88, 128)
    put(sector, 92, 1000)
    put(sector, 96, 2)
    put(sector, 104, 0x100, 2)
    sector[108], sector[109], sector[110] = 9, 7, 1
    return sector


def ntfs():
    sector = boot()
    sector[3:11] = b'NTFS    '
    put(sector, 11, 512, 2)
    sector[13] = 8
    sector[21] = 0xf8
    put(sector, 40, 1000000, 8)
    put(sector, 48, 4, 8)
    put(sector, 56, 8, 8)
    return sector


def mbr(entries=()):
    sector = bytearray(512)
    sector[510:] = b'\x55\xaa'
    for index, (kind, start, count, active) in enumerate(entries):
        offset = 446 + index * 16
        sector[offset], sector[offset + 4] = active, kind
        put(sector, offset + 8, start)
        put(sector, offset + 12, count)
    return sector


def gpt():
    sector = bytearray(512)
    sector[:8] = b'EFI PART'
    put(sector, 8, 0x10000)
    put(sector, 12, 92)
    put(sector, 24, 1, 8)
    put(sector, 32, 999999, 8)
    put(sector, 40, 34, 8)
    put(sector, 48, 999966, 8)
    put(sector, 72, 2, 8)
    put(sector, 80, 128)
    put(sector, 84, 128)
    return sector


class StorageTests(unittest.TestCase):
    def detect(self, sector, size=None):
        raw = bytes(sector)
        return self.lib.aura_storage_format_detect(raw, len(raw) if size is None else size)

    def partition(self, sector, index=0, total=1000000, size=None):
        lba = ctypes.c_uint32(0xdeadbeef)
        raw = bytes(sector)
        ok = self.lib.aura_storage_partition_get(raw, len(raw) if size is None else size, index,
                                               total, ctypes.byref(lba))
        return ok, lba.value

    def test_names_and_unknown_enum(self):
        for value, name in enumerate(['Desconocido', 'FAT12', 'FAT16', 'FAT32', 'exFAT', 'NTFS', 'GPT']):
            self.assertEqual(self.lib.aura_storage_format_name(value).decode(), name)
        self.assertEqual(self.lib.aura_storage_format_name(999).decode(), 'Desconocido')
        self.assertEqual(self.lib.aura_storage_format_name(-1).decode(), 'Desconocido')

    def test_valid_headers_and_fat_thresholds(self):
        for clusters, expected in [(1, FAT12), (4084, FAT12), (4085, FAT16),
                                   (65524, FAT16), (65525, FAT32), (1000000, FAT32)]:
            with self.subTest(clusters=clusters):
                self.assertEqual(self.detect(fat(clusters)), expected)
        self.assertEqual(self.detect(exfat()), EXFAT)
        self.assertEqual(self.detect(ntfs()), NTFS)
        for bps in (512, 1024, 2048, 4096):
            self.assertEqual(self.detect(fat(100000, bps, 8)), FAT32)

    def test_fat_labels_do_not_determine_type(self):
        sector = fat(60000)
        sector[54:62] = b'FAT32   '
        self.assertEqual(self.detect(sector), FAT16)
        sector = fat(100000)
        sector[82:90] = b'FAT12   '
        self.assertEqual(self.detect(sector), FAT32)

    def test_fat_bpb_corruption_and_arithmetic_underflow(self):
        changes = [(0, b'\x00'), (2, b'\x00'), (11, b'\x00\x00'), (11, b'\x00\x03'),
                   (13, b'\x00'), (13, b'\x03'), (14, b'\x00\x00'), (16, b'\x00'),
                   (16, b'\xff'), (21, b'\x01'), (17, b'\x00\x00'), (22, b'\x01\x00'),
                   (510, b'\x00\x00')]
        for offset, data in changes:
            sector = fat(50000)
            sector[offset:offset + len(data)] = data
            with self.subTest(offset=offset, data=data):
                self.assertEqual(self.detect(sector), UNKNOWN)
        sector = fat(50000)
        put(sector, 19, 1, 2)
        self.assertEqual(self.detect(sector), UNKNOWN)
        sector = fat(50000)
        put(sector, 32, 0xffffffff)
        self.assertEqual(self.detect(sector), UNKNOWN)
        sector = fat(100000)
        put(sector, 36, 0xffffffff)
        self.assertEqual(self.detect(sector), UNKNOWN)
        sector = fat(100000)
        put(sector, 44, 100002)
        self.assertEqual(self.detect(sector), UNKNOWN)
        sector = fat(100000)
        put(sector, 42, 1, 2)
        self.assertEqual(self.detect(sector), UNKNOWN)

    def test_exfat_exact_oem_and_structural_limits(self):
        for offset, data in [(3, b'EXFAT  X'), (11, b'\x01'), (108, b'\x08'),
                             (108, b'\x0d'), (109, b'\xff'), (110, b'\x00'),
                             (110, b'\x03'), (104, b'\x00\x00'), (510, b'\x00')]:
            sector = exfat()
            sector[offset:offset + len(data)] = data
            with self.subTest(offset=offset):
                self.assertEqual(self.detect(sector), UNKNOWN)
        for offset, value, width in [(72, 100, 8), (80, 1, 4), (84, 0, 4),
                                     (84, 0xffffffff, 4), (88, 1, 4), (92, 0, 4),
                                     (92, 0xffffffff, 4), (96, 1002, 4)]:
            sector = exfat()
            put(sector, offset, value, width)
            with self.subTest(offset=offset, value=value):
                self.assertEqual(self.detect(sector), UNKNOWN)
        sector = exfat()
        put(sector, 72, 0xffffffffffffffff, 8)
        self.assertEqual(self.detect(sector), EXFAT)

    def test_ntfs_exact_oem_and_cluster_encoding(self):
        for offset, data in [(3, b'NTFS   X'), (11, b'\x00\x00'), (13, b'\x00'),
                             (13, b'\x03'), (13, b'\x81'), (14, b'\x01'),
                             (22, b'\x01'), (32, b'\x01'), (40, b'\x00' * 8)]:
            sector = ntfs()
            sector[offset:offset + len(data)] = data
            with self.subTest(offset=offset):
                self.assertEqual(self.detect(sector), UNKNOWN)
        sector = ntfs()
        sector[13] = 0xf8  # signed -8: 256 sectors per cluster
        self.assertEqual(self.detect(sector), NTFS)
        put(sector, 48, 0xffffffffffffffff, 8)
        self.assertEqual(self.detect(sector), UNKNOWN)

    def test_gpt_header_and_protective_mbr(self):
        self.assertEqual(self.detect(gpt()), GPT)
        self.assertEqual(self.detect(mbr([(0xee, 1, 999999, 0)])), GPT)
        self.assertEqual(self.detect(mbr([(0xee, 1, 0xffffffff, 0)])), GPT)
        for entries in [[(0xee, 0, 99999, 0)], [(0xee, 1, 0, 0)],
                        [(0xee, 1, 100, 0), (7, 101, 100, 0)],
                        [(0xee, 1, 100, 0), (0xee, 1, 100, 0)], [(0xee, 1, 100, 0x80)]]:
            self.assertEqual(self.detect(mbr(entries)), UNKNOWN)
        for offset, value, width in [(8, 0, 4), (12, 8, 4), (12, 513, 4),
                                     (20, 1, 4), (24, 2, 8), (32, 1, 8),
                                     (40, 1000000, 8), (48, 2, 8), (72, 1, 8),
                                     (80, 0, 4), (84, 0, 4), (84, 192, 4)]:
            sector = gpt()
            put(sector, offset, value, width)
            with self.subTest(offset=offset, value=value):
                self.assertEqual(self.detect(sector), UNKNOWN)

    def test_primary_mbr_partition_extents(self):
        sector = mbr([(0x0c, 2048, 500000, 0x80), (7, 502048, 497952, 0),
                      (0x06, 1, 999, 0), (0x0b, 1000, 1048, 0)])
        for index, expected in enumerate([2048, 502048, 1, 1000]):
            self.assertEqual(self.partition(sector, index), (True, expected))
        for index in (4, 0xffffffff):
            self.assertEqual(self.partition(sector, index), (False, 0))
        self.assertEqual(self.partition(mbr([(0xee, 1, 999999, 0)])), (False, 0))
        self.assertEqual(self.partition(mbr()), (False, 0))
        for entries in [[(7, 0, 100, 0)], [(7, 1, 0, 0)], [(0, 1, 100, 0)],
                        [(7, 1, 100, 1)], [(7, 999999, 2, 0)], [(7, 1000000, 1, 0)]]:
            self.assertEqual(self.partition(mbr(entries)), (False, 0))
        self.assertEqual(self.partition(sector, total=0), (False, 0))
        sector[510] = 0
        self.assertEqual(self.partition(sector), (False, 0))

    def test_mbr_32bit_addition_never_wraps(self):
        sector = mbr([(7, 0xfffffff0, 0x20, 0)])
        self.assertEqual(self.partition(sector, total=0xffffffff), (False, 0))
        self.assertEqual(self.partition(sector, total=0x100000010), (True, 0xfffffff0))
        sector = mbr([(7, 0xffffffff, 0xffffffff, 0)])
        self.assertEqual(self.partition(sector, total=0xffffffffffffffff), (True, 0xffffffff))
        self.assertEqual(self.partition(sector, total=0xfffffffe), (False, 0))

    def test_mbr_random_reference_capacity_bounds(self):
        rng = random.Random(20261003)
        for _ in range(3000):
            start, count = rng.getrandbits(32), rng.getrandbits(32)
            total = rng.randrange(0, 1 << 34)
            expected = bool(start and count and start + count <= total)
            ok, lba = self.partition(mbr([(7, start, count, 0)]), total=total)
            self.assertEqual(ok, expected)
            self.assertEqual(lba, start if expected else 0)

    def test_truncated_unaligned_and_read_only(self):
        fixtures = [fat(100000), exfat(), ntfs(), gpt(), mbr([(7, 1, 10, 0)])]
        for sector in fixtures:
            for length in range(512):
                self.assertEqual(self.detect(sector[:length]), UNKNOWN)
                self.assertEqual(self.partition(sector[:length]), (False, 0))
            original = bytes(sector)
            raw = (ctypes.c_uint8 * 514).from_buffer_copy(b'X' + original + b'Y')
            pointer = ctypes.byref(raw, 1)
            self.lib.aura_storage_format_detect(pointer, 512)
            self.assertEqual(bytes(raw), b'X' + original + b'Y')
        self.assertEqual(self.lib.aura_storage_format_detect(None, 512), UNKNOWN)
        self.assertFalse(self.lib.aura_storage_partition_get(None, 512, 0, 1000, None))

    def test_random_headers_and_mutations_are_bounded(self):
        rng = random.Random(13)
        for _ in range(5000):
            sector = bytearray(rng.randbytes(rng.randint(0, 520)))
            value = self.detect(sector)
            self.assertIn(value, range(7))
            self.partition(sector, index=rng.randrange(0, 6), total=rng.getrandbits(64))
        for fixture in (fat(100000), exfat(), ntfs(), gpt()):
            for _ in range(500):
                sector = bytearray(fixture)
                for _ in range(rng.randint(1, 8)):
                    sector[rng.randrange(512)] = rng.randrange(256)
                self.assertIn(self.detect(sector), range(7))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', required=True)
    config, remaining = parser.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='aura-storage-tests-') as directory:
        library = pathlib.Path(directory) / 'storage_format.dll'
        subprocess.run([config.cc, 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared',
                        '-O2', '-o', str(library), str(ROOT / 'main/aura_storage_format.c')], check=True)
        dll = ctypes.CDLL(str(library))
        dll.aura_storage_format_detect.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        dll.aura_storage_format_detect.restype = ctypes.c_int
        dll.aura_storage_format_name.argtypes = [ctypes.c_int]
        dll.aura_storage_format_name.restype = ctypes.c_char_p
        dll.aura_storage_partition_get.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint,
                                                  ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint32)]
        dll.aura_storage_partition_get.restype = ctypes.c_bool
        StorageTests.lib = dll
        result = unittest.main(argv=['test_storage_format.py', *remaining], exit=False)
        import _ctypes
        _ctypes.FreeLibrary(dll._handle)
        raise SystemExit(not result.result.wasSuccessful())
