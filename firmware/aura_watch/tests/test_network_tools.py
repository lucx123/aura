"""Exercise the production network diagnostics as native C, without a radio.

python tests/test_network_tools.py --cc path/to/zig.exe
Uses Python ipaddress as an independent reference for CIDR arithmetic.
"""
import argparse
import ctypes
import ipaddress
import pathlib
import random
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Network(ctypes.Structure):
    _fields_ = [('ssid', ctypes.c_char * 33), ('rssi', ctypes.c_int8),
                ('channel', ctypes.c_uint8), ('authmode', ctypes.c_uint8),
                ('bssid', ctypes.c_uint8 * 6)]


class Scan(ctypes.Structure):
    _fields_ = [('scanning', ctypes.c_bool), ('generation', ctypes.c_uint32),
                ('count', ctypes.c_uint16), ('error', ctypes.c_int32),
                ('networks', Network * 24)]


class Report(ctypes.Structure):
    _fields_ = [('sample_count', ctypes.c_uint16), ('channel_count', ctypes.c_uint16 * 14),
                ('channel_score', ctypes.c_uint32 * 14), ('unmodelled_channels', ctypes.c_uint16),
                ('open_count', ctypes.c_uint16), ('legacy_count', ctypes.c_uint16),
                ('wpa2_count', ctypes.c_uint16), ('wpa3_count', ctypes.c_uint16),
                ('transition_count', ctypes.c_uint16), ('owe_count', ctypes.c_uint16),
                ('other_count', ctypes.c_uint16), ('best_channel', ctypes.c_uint8),
                ('best_channel_mask', ctypes.c_uint16)]


class CIDR(ctypes.Structure):
    _fields_ = [('address', ctypes.c_uint32), ('network', ctypes.c_uint32),
                ('netmask', ctypes.c_uint32), ('last_address', ctypes.c_uint32),
                ('first_host', ctypes.c_uint32), ('last_host', ctypes.c_uint32),
                ('host_count', ctypes.c_uint64), ('prefix', ctypes.c_uint8),
                ('has_broadcast', ctypes.c_bool)]


def snapshot(generation=1, rows=()):
    scan = Scan(generation=generation, count=len(rows))
    for index, (ssid, rssi, channel, authmode) in enumerate(rows):
        scan.networks[index] = Network(ssid.encode(), rssi, channel, authmode)
    return scan


class NetworkToolsTests(unittest.TestCase):
    def setUp(self):
        self.lib.aura_network_tools_session_stop()

    def parse(self, cidr):
        result = CIDR()
        ok = self.lib.aura_network_tools_cidr_parse(cidr.encode(), ctypes.byref(result))
        return ok, result

    def analyze(self, scan):
        report = Report()
        self.lib.aura_network_tools_analyze(ctypes.byref(scan), ctypes.byref(report))
        return report

    def text(self, name, capacity=2400, cidr=None):
        output = ctypes.create_string_buffer(capacity)
        args = (output, capacity) if cidr is None else (cidr.encode(), output, capacity)
        ok = getattr(self.lib, name)(*args)
        return ok, output.value.decode()

    def test_cidr_known_and_point_to_point(self):
        for cidr, expected in [('192.168.1.10/24', (3232235776, 3232236031, 254, True)),
                               ('10.0.0.15/30', (167772172, 167772175, 2, True)),
                               ('10.0.0.3/31', (167772162, 167772163, 2, False)),
                               ('10.0.0.3/32', (167772163, 167772163, 1, False)),
                               ('0.0.0.0/0', (0, 4294967295, 4294967294, True))]:
            with self.subTest(cidr=cidr):
                ok, result = self.parse(cidr)
                self.assertTrue(ok)
                self.assertEqual((result.network, result.last_address, result.host_count,
                                  result.has_broadcast), expected)

    def test_cidr_reference_for_every_prefix(self):
        rng = random.Random(20261002)
        for prefix in range(33):
            for _ in range(70):
                address = rng.getrandbits(32)
                cidr = f'{ipaddress.IPv4Address(address)}/{prefix}'
                expected = ipaddress.IPv4Interface(cidr)
                ok, result = self.parse(cidr)
                self.assertTrue(ok)
                self.assertEqual(result.address, address)
                self.assertEqual(result.prefix, prefix)
                self.assertEqual(result.netmask, int(expected.netmask))
                self.assertEqual(result.network, int(expected.network.network_address))
                self.assertEqual(result.last_address, int(expected.network.broadcast_address))
                count = expected.network.num_addresses - (2 if prefix < 31 else 0)
                self.assertEqual(result.host_count, count)
                self.assertEqual(result.first_host, result.network + (1 if prefix < 31 else 0))
                self.assertEqual(result.last_host, result.last_address - (1 if prefix < 31 else 0))

    def test_cidr_rejects_malformed_without_partial_result(self):
        invalid = ['', '192.168.1.1', '192.168.1.1/33', '256.1.1.1/24', '1.1.1.1/-1',
                   '1.1.1.1/1x', '1.1.1.1/024', '1.1.1.01/24', ' 1.1.1.1/24',
                   '1.1.1.1/24 ', '1.1.1/24', '1..1.1/24', '1.1.1.1.1/24',
                   '4294967296.1.1.1/1', '1.1.1.1/32\n', '1.1.1.1/']
        for cidr in invalid:
            with self.subTest(cidr=cidr):
                ok, result = self.parse(cidr)
                self.assertFalse(ok)
                self.assertEqual(bytes(result), bytes(ctypes.sizeof(result)))

    def test_channel_estimate_known_order_and_ties(self):
        scan = snapshot(rows=[('A', -50, 1, 3), ('B', -90, 6, 3), ('C', -40, 11, 3)])
        report = self.analyze(scan)
        self.assertEqual([report.channel_score[x] for x in (1, 6, 11)], [230, 30, 280])
        self.assertEqual(report.channel_score[2], 190)
        self.assertEqual(report.best_channel, 6)
        self.assertEqual(report.best_channel_mask, 1 << 6)
        report = self.analyze(snapshot(rows=[('A', -50, 1, 3)]))
        self.assertEqual(report.best_channel, 6)
        self.assertEqual(report.best_channel_mask, (1 << 6) | (1 << 11))
        self.assertEqual(self.analyze(snapshot()).best_channel, 0)

    def test_channel_boundaries_clamping_and_limited_snapshot(self):
        scan = snapshot(rows=[('A', -128, 1, 3), ('B', 127, 13, 3), ('C', -20, 14, 0)])
        report = self.analyze(scan)
        self.assertEqual(report.channel_score[1], 5)
        self.assertEqual(report.channel_score[13], 305)
        self.assertEqual(report.channel_score[11], 183)
        self.assertEqual(report.unmodelled_channels, 1)
        scan.count = 65535
        report = self.analyze(scan)
        self.assertEqual(report.sample_count, 24)
        self.assertEqual(self.analyze(snapshot(rows=[('A', -60, 14, 3)])).best_channel, 0)
        scan.scanning = True
        self.assertEqual(self.analyze(scan).sample_count, 0)

    def test_authentication_categories_cover_sdk55_values(self):
        scan = snapshot(rows=[(str(auth), -60, 1, auth) for auth in range(17)] + [('Unknown', -60, 6, 255)])
        report = self.analyze(scan)
        self.assertEqual((report.open_count, report.legacy_count, report.wpa2_count, report.wpa3_count,
                          report.transition_count, report.owe_count, report.other_count),
                         (1, 4, 2, 5, 2, 1, 3))
        self.assertEqual(sum([report.open_count, report.legacy_count, report.wpa2_count, report.wpa3_count,
                              report.transition_count, report.owe_count, report.other_count]), report.sample_count)

    def test_session_exit_stale_scan_error_and_reentry(self):
        lib = self.lib
        scan = snapshot(7, [('Before', -40, 1, 0)])
        self.assertFalse(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        self.assertFalse(self.text('aura_network_tools_format_security')[0])
        lib.aura_network_tools_session_start(7)
        self.assertTrue(lib.aura_network_tools_session_active())
        self.assertFalse(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        scan.generation = 8
        scan.scanning = True
        self.assertFalse(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        scan.scanning = False
        self.assertTrue(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        self.assertIn('Before', self.text('aura_network_tools_format_security')[1])
        scan.generation = 6
        self.assertFalse(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        scan.generation = 9
        scan.error = 0x1234
        self.assertTrue(lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        ok, text = self.text('aura_network_tools_format_security')
        self.assertFalse(ok)
        self.assertNotIn('Before', text)
        self.assertIn('0x1234', text)
        lib.aura_network_tools_session_stop()
        self.assertFalse(lib.aura_network_tools_session_active())
        lib.aura_network_tools_session_start(9)
        self.assertNotIn('Before', self.text('aura_network_tools_format_security')[1])

    def test_scan_generation_wrap(self):
        self.lib.aura_network_tools_session_start(0xffffffff)
        scan = snapshot(0, [('Wrapped', -50, 11, 7)])
        self.assertTrue(self.lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        scan.generation = 0xffffffff
        self.assertFalse(self.lib.aura_network_tools_update_scan(ctypes.byref(scan)))

    def test_spectrum_session_freshness_sanitization_and_bounds(self):
        name = 'aura_network_tools_format_spectrum'
        self.assertFalse(self.text(name)[0])
        self.lib.aura_network_tools_session_start(7)
        old = snapshot(7, [('OLD', -40, 1, 0)])
        self.assertFalse(self.lib.aura_network_tools_update_scan(ctypes.byref(old)))
        self.assertNotIn('OLD', self.text(name)[1])
        new = snapshot(8, [('NEW\nFORGED\t', -65, 6, 7)])
        self.assertTrue(self.lib.aura_network_tools_update_scan(ctypes.byref(new)))
        ok, text = self.text(name)
        self.assertTrue(ok)
        self.assertIn('1 AP observados', text)
        self.assertIn('NEW?FORGED?', text)
        self.assertIn('CH 6 | -65 dBm | WPA2/WPA3', text)
        for capacity in (0, 1, 2, 10, 20):
            raw = (ctypes.c_ubyte * (capacity + 12))(*([0x42] * (capacity + 12)))
            self.assertFalse(getattr(self.lib, name)(raw, capacity))
            self.assertEqual(list(raw)[capacity:], [0x42] * 12)
            if capacity:
                self.assertEqual(raw[capacity - 1], 0)
        self.lib.aura_network_tools_session_stop()
        self.lib.aura_network_tools_session_start(8)
        self.assertNotIn('NEW', self.text(name)[1])

    def test_cidr_session_guard_and_useful_output(self):
        name = 'aura_network_tools_calculate_cidr'
        self.assertFalse(self.text(name, cidr='1.1.1.1/24')[0])
        self.lib.aura_network_tools_session_start(0)
        ok, text = self.text(name, cidr='192.168.1.10/24')
        self.assertTrue(ok)
        self.assertIn('Red: 192.168.1.0/24', text)
        self.assertIn('Broadcast: 192.168.1.255', text)
        self.assertIn('Hosts: 254', text)
        self.assertIn('RFC 3021', self.text(name, cidr='10.0.0.3/31')[1])
        self.assertFalse(self.text(name, cidr='1.1.1.256/24')[0])

    def test_bssid_distinguishes_same_ssid_and_fits_maximum_scan(self):
        self.lib.aura_network_tools_session_start(0)
        scan = snapshot(1, [('SAME', -65, 6, 3), ('SAME', -70, 6, 3)])
        scan.networks[0].bssid[:] = [0x02, 0x11, 0x22, 0x33, 0x44, 0xAA]
        scan.networks[1].bssid[:] = [0x02, 0x11, 0x22, 0x33, 0x44, 0xBB]
        self.assertTrue(self.lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        ok, text = self.text('aura_network_tools_format_spectrum')
        self.assertTrue(ok)
        self.assertIn('BSSID 02:11:22:33:44:AA', text)
        self.assertIn('BSSID 02:11:22:33:44:BB', text)
        scan = snapshot(2, [('X' * 32, -128, 255, 10)] * 24)
        self.assertTrue(self.lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        ok, text = self.text('aura_network_tools_format_spectrum')
        self.assertTrue(ok)
        self.assertEqual(text.count('BSSID '), 24)
        self.assertLess(len(text.encode()), 2400)

    def test_outputs_are_bounded_and_ssid_controls_sanitized(self):
        self.lib.aura_network_tools_session_start(0)
        scan = snapshot(1, [('NAME\nFAKE\t', -65, 3, 0)] + [('X' * 32, -50, 11, 10)] * 23)
        self.assertTrue(self.lib.aura_network_tools_update_scan(ctypes.byref(scan)))
        ok, text = self.text('aura_network_tools_format_security')
        self.assertTrue(ok)
        self.assertIn('NAME?FAKE?', text)
        self.assertNotIn('NAME\nFAKE', text)
        for function in ('aura_network_tools_format_security', 'aura_network_tools_format_channels'):
            for capacity in (0, 1, 2, 10, 60):
                raw = (ctypes.c_ubyte * (capacity + 12))(*([0x42] * (capacity + 12)))
                ok = getattr(self.lib, function)(raw, capacity)
                self.assertFalse(ok)
                self.assertEqual(list(raw)[capacity:], [0x42] * 12)
                if capacity:
                    self.assertEqual(raw[capacity - 1], 0)


if __name__ == '__main__':
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--cc', required=True)
    config, remaining = arguments.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='aura-network-tests-') as directory:
        path = pathlib.Path(directory)
        (path / 'esp_err.h').write_text('#include <stdint.h>\ntypedef int32_t esp_err_t;\n#define ESP_OK 0\n')
        # Named enums mirror the supported ESP-IDF 5.5 public auth-mode values.
        # Production compiles against the SDK's real esp_wifi_types.h.
        (path / 'esp_wifi_types.h').write_text('''typedef enum {
            WIFI_AUTH_OPEN=0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
            WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_ENTERPRISE,
            WIFI_AUTH_WPA2_ENTERPRISE=WIFI_AUTH_ENTERPRISE, WIFI_AUTH_WPA3_PSK,
            WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WAPI_PSK, WIFI_AUTH_OWE,
            WIFI_AUTH_WPA3_ENT_192, WIFI_AUTH_WPA3_EXT_PSK,
            WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE, WIFI_AUTH_DPP,
            WIFI_AUTH_WPA3_ENTERPRISE, WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
            WIFI_AUTH_WPA_ENTERPRISE, WIFI_AUTH_MAX } wifi_auth_mode_t;''')
        library = path / 'network_tools.dll'
        subprocess.run([config.cc, 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-O2', '-I', str(path), '-o', str(library),
                        str(ROOT / 'main/aura_network_tools.c')], check=True)
        dll = ctypes.CDLL(str(library))
        dll.aura_network_tools_analyze.argtypes = [ctypes.POINTER(Scan), ctypes.POINTER(Report)]
        dll.aura_network_tools_cidr_parse.argtypes = [ctypes.c_char_p, ctypes.POINTER(CIDR)]
        dll.aura_network_tools_cidr_parse.restype = ctypes.c_bool
        dll.aura_network_tools_session_start.argtypes = [ctypes.c_uint32]
        dll.aura_network_tools_session_stop.argtypes = []
        dll.aura_network_tools_session_active.argtypes = []
        dll.aura_network_tools_session_active.restype = ctypes.c_bool
        dll.aura_network_tools_update_scan.argtypes = [ctypes.POINTER(Scan)]
        dll.aura_network_tools_update_scan.restype = ctypes.c_bool
        for name in ('aura_network_tools_format_channels', 'aura_network_tools_format_security',
                     'aura_network_tools_format_spectrum'):
            function = getattr(dll, name)
            function.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
            function.restype = ctypes.c_bool
        dll.aura_network_tools_calculate_cidr.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
        dll.aura_network_tools_calculate_cidr.restype = ctypes.c_bool
        NetworkToolsTests.lib = dll
        result = unittest.main(argv=['test_network_tools.py', *remaining], exit=False)
        import _ctypes
        _ctypes.FreeLibrary(dll._handle)
        raise SystemExit(not result.result.wasSuccessful())
