"""Compile the production offline calculators and compare independent references.

python tests/test_engineer_tools.py --cc path/to/zig.exe
No board, radio, network or credentials required.
"""
import argparse
import ctypes
import ipaddress
import math
import pathlib
import random
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RF(ctypes.Structure):
    _fields_ = [('frequency_mhz', ctypes.c_double), ('distance_m', ctypes.c_double),
                ('power_dbm', ctypes.c_double), ('wavelength_m', ctypes.c_double),
                ('fspl_db', ctypes.c_double), ('power_mw', ctypes.c_double),
                ('short_distance', ctypes.c_bool)]


class Assignment(ctypes.Structure):
    _fields_ = [('requested_hosts', ctypes.c_uint32), ('network', ctypes.c_uint32),
                ('broadcast', ctypes.c_uint32), ('first_host', ctypes.c_uint32),
                ('last_host', ctypes.c_uint32), ('capacity_hosts', ctypes.c_uint32),
                ('prefix', ctypes.c_uint8), ('request_index', ctypes.c_uint8)]


class VLSM(ctypes.Structure):
    _fields_ = [('error', ctypes.c_int), ('base_network', ctypes.c_uint32),
                ('base_prefix', ctypes.c_uint8), ('count', ctypes.c_uint8),
                ('remaining_addresses', ctypes.c_uint64), ('assignments', Assignment * 4)]


def reference_plan(cidr, requests):
    base = ipaddress.IPv4Network(cidr)
    cursor = int(base.network_address)
    end = int(base.broadcast_address) + 1
    result = []
    for index, hosts in sorted(enumerate(requests, 1), key=lambda item: -item[1]):
        prefix = 32 - (hosts + 1).bit_length()
        prefix = min(prefix, 30)
        size = 1 << (32 - prefix)
        if cursor + size > end:
            return None
        network = ipaddress.IPv4Network((cursor, prefix))
        result.append((index, hosts, network, size - 2))
        cursor += size
    return result, end - cursor


class EngineerTests(unittest.TestCase):
    def rf(self, text):
        result = RF()
        ok = self.lib.aura_engineer_tools_rf_calculate(text.encode(), ctypes.byref(result))
        return ok, result

    def vlsm(self, text):
        result = VLSM()
        ok = self.lib.aura_engineer_tools_vlsm_calculate(text.encode(), ctypes.byref(result))
        return ok, result

    def output(self, function, text, capacity=1600):
        output = ctypes.create_string_buffer(capacity)
        ok = getattr(self.lib, function)(text.encode(), output, capacity)
        return ok, output.value.decode()

    def test_rf_known_values(self):
        ok, result = self.rf('2400,100,-20')
        self.assertTrue(ok)
        self.assertAlmostEqual(result.wavelength_m, 0.12491352416666667, places=12)
        self.assertAlmostEqual(result.fspl_db, 80.0520080561155, places=9)
        self.assertAlmostEqual(result.power_mw, 0.01, places=12)
        self.assertFalse(result.short_distance)
        for dbm, mw in [('-30', 0.001), ('0', 1), ('10', 10), ('20', 100), ('30', 1000)]:
            self.assertAlmostEqual(self.rf(f'1000,1000,{dbm}')[1].power_mw, mw, places=9)
        self.assertTrue(self.rf('1,1,0')[1].short_distance)

    def test_rf_reference_random_decimals(self):
        rng = random.Random(20261005)
        for _ in range(1000):
            frequency = rng.randint(1, 10000000) / 100
            distance = rng.randint(1, 10000000) / 100
            power = rng.randint(-20000, 10000) / 100
            text = f'{frequency:.2f},{distance:.2f},{power:.2f}'
            ok, result = self.rf(text)
            self.assertTrue(ok)
            # Independent convenient-units form: MHz/km, exact c-derived constant.
            constant = 20 * math.log10(4 * math.pi * 1e9 / 299792458)
            expected = constant + 20 * math.log10(frequency) + 20 * math.log10(distance / 1000)
            self.assertAlmostEqual(result.fspl_db, expected, places=9)
            self.assertAlmostEqual(result.wavelength_m, 299.792458 / frequency, places=9)
            self.assertTrue(math.isclose(result.power_mw, 10 ** (power / 10), rel_tol=1e-12))

    def test_rf_range_endpoints_and_rejections(self):
        for text in ['0.001,0.001,-200', '100000,10000000,100', '2400.123456,100.123456,-20.123456']:
            with self.subTest(text=text):
                ok, result = self.rf(text)
                self.assertTrue(ok)
                self.assertTrue(all(math.isfinite(getattr(result, field)) for field, _ in RF._fields_[:-1]))
        invalid = ['', '2400,100', '2400,100,-20,3', '2400,0,-20', '-2400,100,-20',
                   '0,100,0', '0.000999,100,0', '100000.1,100,0', '1,0.000999,0',
                   '1,10000000.1,0', '1,1,-200.1', '1,1,100.1', 'nan,1,0',
                   '1,inf,0', '1,1,NaN', '1e3,1,0', '0x10,1,0', '+1,1,0',
                   '1, 1,0', '1,1,0 ', '1,1,.1', '1,1,1.', '01,1,0',
                   '1,1,1.1234567', '999999999999999999999999999,1,0', '1' * 81]
        for text in invalid:
            with self.subTest(text=text):
                ok, result = self.rf(text)
                self.assertFalse(ok)
                self.assertEqual(bytes(result), bytes(ctypes.sizeof(result)))

    def test_vlsm_known_and_original_request_identity(self):
        ok, result = self.vlsm('192.168.1.0/24,10,50,20')
        self.assertTrue(ok)
        self.assertEqual(result.count, 3)
        self.assertEqual(result.remaining_addresses, 144)
        expected = [(2, 50, '192.168.1.0/26', 62),
                    (3, 20, '192.168.1.64/27', 30),
                    (1, 10, '192.168.1.96/28', 14)]
        for assignment, (index, hosts, cidr, capacity) in zip(result.assignments, expected):
            network = ipaddress.IPv4Network(cidr)
            self.assertEqual(assignment.request_index, index)
            self.assertEqual(assignment.requested_hosts, hosts)
            self.assertEqual(assignment.network, int(network.network_address))
            self.assertEqual(assignment.broadcast, int(network.broadcast_address))
            self.assertEqual(assignment.first_host, int(network.network_address) + 1)
            self.assertEqual(assignment.last_host, int(network.broadcast_address) - 1)
            self.assertEqual(assignment.capacity_hosts, capacity)
            self.assertEqual(assignment.prefix, network.prefixlen)

    def test_vlsm_independent_random_allocation_and_nonoverlap(self):
        rng = random.Random(525)
        for _ in range(1200):
            prefix = rng.randint(8, 30)
            base = ipaddress.IPv4Network((rng.getrandbits(32), prefix), strict=False)
            requests = [rng.randint(1, 4096) for _ in range(rng.randint(1, 4))]
            expected = reference_plan(str(base), requests)
            ok, result = self.vlsm(str(base) + ',' + ','.join(map(str, requests)))
            self.assertEqual(ok, expected is not None)
            if expected is None:
                self.assertEqual(result.error, 3)
                self.assertEqual(result.count, 0)
                continue
            assignments, remaining = expected
            self.assertEqual(result.count, len(assignments))
            self.assertEqual(result.remaining_addresses, remaining)
            previous_end = int(base.network_address) - 1
            for actual, (index, hosts, network, capacity) in zip(result.assignments, assignments):
                self.assertEqual(actual.request_index, index)
                self.assertEqual(actual.requested_hosts, hosts)
                self.assertEqual(actual.network, int(network.network_address))
                self.assertEqual(actual.broadcast, int(network.broadcast_address))
                self.assertGreater(actual.network, previous_end)
                previous_end = actual.broadcast
                self.assertEqual(actual.capacity_hosts, capacity)
                self.assertEqual(actual.prefix, network.prefixlen)
                self.assertTrue(network.subnet_of(base))

    def test_vlsm_high_addresses_full_ipv4_and_ties(self):
        ok, result = self.vlsm('255.255.255.252/30,2')
        self.assertTrue(ok)
        self.assertEqual(result.assignments[0].broadcast, 0xffffffff)
        self.assertEqual(result.remaining_addresses, 0)
        ok, result = self.vlsm('0.0.0.0/0,4294967294')
        self.assertTrue(ok)
        self.assertEqual(result.assignments[0].prefix, 0)
        self.assertEqual(result.assignments[0].capacity_hosts, 4294967294)
        self.assertEqual(result.remaining_addresses, 0)
        ok, result = self.vlsm('0.0.0.0/0,2147483646,2147483646')
        self.assertTrue(ok)
        self.assertEqual(result.assignments[1].network, 0x80000000)
        self.assertEqual(result.assignments[1].broadcast, 0xffffffff)
        self.assertEqual(result.remaining_addresses, 0)
        ok, result = self.vlsm('10.0.0.0/28,1,1,2,2')
        self.assertTrue(ok)
        self.assertEqual([row.request_index for row in result.assignments], [3, 4, 1, 2])

    def test_vlsm_no_partial_result_or_wrap_on_failure(self):
        for text in ['255.255.255.252/30,3', '255.255.255.252/30,2,2',
                     '0.0.0.0/0,4294967294,1', '0.0.0.0/1,4294967294',
                     '192.168.1.0/24,200,100']:
            with self.subTest(text=text):
                ok, result = self.vlsm(text)
                self.assertFalse(ok)
                self.assertEqual(result.error, 3)
                self.assertEqual(result.count, 0)
                self.assertEqual(result.remaining_addresses, 0)
                self.assertEqual(bytes(result.assignments), bytes(ctypes.sizeof(result.assignments)))

    def test_vlsm_strict_input(self):
        invalid = ['', '192.168.1.0/24', '192.168.1.0/24,', '192.168.1.0/24,0',
                   '192.168.1.0/24,-1', '192.168.1.0/24,01', '192.168.1.0/24,1.5',
                   '192.168.1.0/24,1,,2', '192.168.1.0/24,1,', '192.168.1.0/24, 1',
                   '192.168.1.0/24,1,2,3,4,5', '192.168.1.0/24,4294967295',
                   '192.168.1.0/24,18446744073709551615', '192.168.1.0/24,1e3',
                   '192.168.1.0/24,NaN', '192.168.1.0/24,1' + '0' * 80]
        for text in invalid:
            with self.subTest(text=text):
                ok, result = self.vlsm(text)
                self.assertFalse(ok)
                self.assertEqual(result.error, 1)
        for text in ['192.168.1.10/24,1', '192.168.1.0/33,1', '10.0.0.0/31,1',
                     '10.0.0.0/32,1', '256.1.1.0/24,1']:
            self.assertEqual(self.vlsm(text)[1].error, 2)

    def test_useful_format_and_limits(self):
        rf = 'aura_engineer_tools_format_rf'
        vlsm = 'aura_engineer_tools_format_vlsm'
        ok, text = self.output(rf, '2400,100,-20')
        self.assertTrue(ok)
        self.assertIn('FSPL: 80.05 dB', text)
        self.assertIn('Potencia: 0.01 mW', text)
        self.assertIn('campo lejano', text)
        self.assertIn('menor que lambda', self.output(rf, '1,1,0')[1])
        ok, text = self.output(vlsm, '192.168.1.0/24,10,50,20')
        self.assertTrue(ok)
        self.assertIn('Solicitud 2: 50 hosts', text)
        self.assertIn('192.168.1.0/26', text)
        self.assertIn('192.168.1.64/27', text)
        self.assertIn('Broadcast: 192.168.1.111', text)
        self.assertIn('Direcciones sin asignar: 144', text)
        self.assertFalse(self.output(vlsm, '10.0.0.0/30,100')[0])
        self.assertIn('Sin espacio', self.output(vlsm, '10.0.0.0/30,100')[1])
        for name, input_text in [(rf, '2400,100,-20'), (rf, 'NaN,1,0'),
                                 (vlsm, '192.168.1.0/24,50,20,10'), (vlsm, 'bad')]:
            for capacity in (0, 1, 2, 10, 50):
                raw = (ctypes.c_ubyte * (capacity + 12))(*([0x42] * (capacity + 12)))
                self.assertFalse(getattr(self.lib, name)(input_text.encode(), raw, capacity))
                self.assertEqual(list(raw)[capacity:], [0x42] * 12)
                if capacity:
                    self.assertEqual(raw[capacity - 1], 0)

    def test_random_input_bytes_are_bounded(self):
        rng = random.Random(20)
        for _ in range(4000):
            raw = rng.randbytes(rng.randint(0, 100))
            rf, vlsm = RF(), VLSM()
            self.lib.aura_engineer_tools_rf_calculate(raw, ctypes.byref(rf))
            self.lib.aura_engineer_tools_vlsm_calculate(raw, ctypes.byref(vlsm))


if __name__ == '__main__':
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--cc', required=True)
    config, remaining = arguments.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='aura-engineer-tests-') as directory:
        path = pathlib.Path(directory)
        (path / 'esp_err.h').write_text('#include <stdint.h>\ntypedef int32_t esp_err_t;\n#define ESP_OK 0\n')
        (path / 'esp_wifi_types.h').write_text('''typedef enum {
            WIFI_AUTH_OPEN=0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
            WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_ENTERPRISE,
            WIFI_AUTH_WPA2_ENTERPRISE=WIFI_AUTH_ENTERPRISE, WIFI_AUTH_WPA3_PSK,
            WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WAPI_PSK, WIFI_AUTH_OWE,
            WIFI_AUTH_WPA3_ENT_192, WIFI_AUTH_WPA3_EXT_PSK,
            WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE, WIFI_AUTH_DPP,
            WIFI_AUTH_WPA3_ENTERPRISE, WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
            WIFI_AUTH_WPA_ENTERPRISE, WIFI_AUTH_MAX } wifi_auth_mode_t;''')
        library = path / 'engineer_tools.dll'
        subprocess.run([config.cc, 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-O2', '-I', str(path), '-o', str(library),
                        str(ROOT / 'main/aura_engineer_tools.c'),
                        str(ROOT / 'main/aura_network_tools.c')], check=True)
        dll = ctypes.CDLL(str(library))
        dll.aura_engineer_tools_rf_calculate.argtypes = [ctypes.c_char_p, ctypes.POINTER(RF)]
        dll.aura_engineer_tools_rf_calculate.restype = ctypes.c_bool
        dll.aura_engineer_tools_vlsm_calculate.argtypes = [ctypes.c_char_p, ctypes.POINTER(VLSM)]
        dll.aura_engineer_tools_vlsm_calculate.restype = ctypes.c_bool
        for name in ('aura_engineer_tools_format_rf', 'aura_engineer_tools_format_vlsm'):
            function = getattr(dll, name)
            function.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
            function.restype = ctypes.c_bool
        EngineerTests.lib = dll
        result = unittest.main(argv=['test_engineer_tools.py', *remaining], exit=False)
        import _ctypes
        _ctypes.FreeLibrary(dll._handle)
        raise SystemExit(not result.result.wasSuccessful())
