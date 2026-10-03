"""Compile the production parser and exercise real C code on the host.

python tests/test_portal.py --cc path/to/zig.exe
Requires Zig (used as a C compiler); no board or Wi-Fi credentials required.
"""
import argparse
import ctypes
import pathlib
import random
import subprocess
import tempfile
import unittest
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parents[1]


class PortalTests(unittest.TestCase):
    def parse(self, body):
        raw = body.encode() if isinstance(body, str) else body
        ssid, password = ctypes.create_string_buffer(33), ctypes.create_string_buffer(65)
        ok = self.parser(raw, len(raw), ssid, password)
        return bool(ok), ssid.value, password.value

    def test_open_and_protected_networks(self):
        self.assertEqual(self.parse('s=Aura&p='), (True, b'Aura', b''))
        self.assertEqual(self.parse('p=12345678&s=Aura+Home'), (True, b'Aura Home', b'12345678'))

    def test_full_length_fields(self):
        for password in ('f' * 64, '!' * 63):
            body = urllib.parse.urlencode({'s': '!' * 32, 'p': password})
            self.assertEqual(self.parse(body), (True, b'!' * 32, password.encode()))

    def test_escaped_separators_and_utf8(self):
        for name in ('A&B=+% red', 'Red de Lucás', 'ñ' * 16):
            body = urllib.parse.urlencode({'s': name, 'p': 'a&=+%12345'})
            self.assertEqual(self.parse(body), (True, name.encode(), b'a&=+%12345'))

    def test_rejects_truncation_and_invalid_forms(self):
        bodies = ['s=&p=', 's=a', 'p=12345678', 's=a&p=short',
                  's=a&s=b&p=', 's=a&p=&p=', 'xs=a&p=', 's=a&p=&',
                  's=a&p=%', 's=a&p=%0', 's=a&p=%GG', 's=a%00x&p=',
                  's=a&p=1234567%00', 's=' + 'a' * 33 + '&p=',
                  's=a&p=' + 'z' * 64, 's=a&p=' + 'f' * 65,
                  urllib.parse.urlencode({'s': 'ñ' * 17, 'p': ''}),
                  b's=a\x00b&p=', 's=a&p=&extra=1']
        for body in bodies:
            with self.subTest(body=body):
                self.assertEqual(self.parse(body)[0], False)

    def test_roundtrip_random_credentials(self):
        rng = random.Random(20261002)
        alphabet = ''.join(chr(i) for i in range(32, 127))
        for _ in range(2000):
            name = ''.join(rng.choices(alphabet, k=rng.randint(1, 32)))
            password = ''.join(rng.choices(alphabet, k=rng.randint(8, 63)))
            self.assertEqual(self.parse(urllib.parse.urlencode({'p': password, 's': name})),
                             (True, name.encode(), password.encode()))

    def test_random_bytes_are_bounded(self):
        rng = random.Random(7)
        for _ in range(10000):
            self.parse(rng.randbytes(rng.randrange(0, 310)))


if __name__ == '__main__':
    args = argparse.ArgumentParser()
    args.add_argument('--cc', required=True)
    config, rest = args.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='aura-parser-') as directory:
        library = pathlib.Path(directory) / 'portal.dll'
        subprocess.run([config.cc, 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-O1', '-o', str(library), str(ROOT / 'main/aura_portal.c')], check=True)
        dll = ctypes.CDLL(str(library))
        dll.aura_portal_parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t,
                                         ctypes.c_void_p, ctypes.c_void_p]
        dll.aura_portal_parse.restype = ctypes.c_bool
        PortalTests.parser = staticmethod(dll.aura_portal_parse)
        result = unittest.main(argv=['test_portal.py', *rest], exit=False)
        # Windows cannot remove a loaded DLL. Release our sole library handle.
        import _ctypes
        _ctypes.FreeLibrary(dll._handle)
        raise SystemExit(not result.result.wasSuccessful())
