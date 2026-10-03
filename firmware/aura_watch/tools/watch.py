"""USB recovery and diagnostics. Requires pyserial; opening does not reset the watch."""
import argparse
import datetime
import ipaddress
import json
import pathlib
import re
import struct
import time
import zlib
import serial

def project_version():
    config = pathlib.Path(__file__).resolve().parents[1] / 'CMakeLists.txt'
    return re.search(r'set\(PROJECT_VER\s+"([^"]+)"\)', config.read_text()).group(1)

class Watch:
    def __init__(self, port='COM4'):
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=2)
        self.port.dtr = False
        self.port.rts = False
        self.port.port = port
        self.port.open()
        self.port.reset_input_buffer()
        self.read_retries = 0

    def close(self):
        self.port.close()

    def send(self, command):
        self.port.write((command + '\n').encode())
        self.port.flush()

    def response(self, prefix, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.port.readline().decode(errors='replace').strip()
            if line.startswith(prefix):
                return line
            if (line.startswith('AURA_ERROR') or 'QSPI flush failed' in line or
                    'QSPI completion timeout' in line or 'AURA_READY' in line):
                raise RuntimeError(line)
        raise TimeoutError(f'No response starting with {prefix}')

    def display(self):
        # Windows can resume USB RX before the device's TX connection monitor.
        # Retry only this read; never repeat a mutation or ignore reboot/errors.
        for attempt in range(2):
            self.send('DISPLAY')
            try:
                line = self.response('AURA_DISPLAY ', timeout=3)
                return dict(item.split('=', 1) for item in line.split()[1:])
            except TimeoutError:
                if attempt: raise
                self.read_retries += 1
                time.sleep(0.2)

    def wait_eclipse_ready(self, timeout=10):
        """Wait for the visible introduction and the final touch guard to finish."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            current = self.display()
            if current['eclipse'] == '1' and current.get('eclipse_intro', '0') == '0':
                time.sleep(0.65)
                return self.display()
            time.sleep(0.1)
        raise TimeoutError(f'Eclipse introduction did not finish: {current}')

    def evidence_export(self, output):
        self.send('EVIDENCE_EXPORT')
        records = []
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            line = self.port.readline().decode(errors='strict').strip()
            if line.startswith('AURA_EVIDENCE '):
                record = line[len('AURA_EVIDENCE '):]
                json.loads(record)
                records.append(record)
                if len(records) > 8: raise RuntimeError('Journal exceeds capacity')
            elif line.startswith('AURA_EVIDENCE_END '):
                fields = dict(item.split('=', 1) for item in line.split()[1:])
                if fields['error'] != 'ESP_OK': raise RuntimeError(line)
                if int(fields['count']) != len(records): raise RuntimeError('Incomplete journal')
                path = pathlib.Path(output)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(''.join(record+'\n' for record in records), encoding='utf-8')
                return path.resolve(), len(records)
            elif line.startswith(('AURA_ERROR', 'AURA_READY')): raise RuntimeError(line)
        raise TimeoutError('Evidence export did not finish')

    def screenshot(self, output):
        self.send('SCREEN')
        header = self.response('AURA_FRAME ')
        _, width, height, stride, size = header.split()
        width, height, stride, size = map(int, (width, height, stride, size))
        if not (0 < width <= 1024 and 0 < height <= 1024 and
                width * 2 <= stride <= width * 2 + 256 and size == stride * height):
            raise ValueError('Invalid frame dimensions: ' + header)
        data = bytearray()
        deadline = time.monotonic() + 15
        while len(data) < size and time.monotonic() < deadline:
            data.extend(self.port.read(min(4096, size - len(data))))
        if len(data) != size:
            raise RuntimeError(f'Incomplete frame {len(data)}/{size}')
        rows = bytearray()
        for y in range(height):
            rows.append(0)
            for x in range(width):
                value = struct.unpack_from('<H', data, y * stride + x * 2)[0]
                rows.extend(((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63,
                             (value & 31) * 255 // 31))
        def chunk(kind, content):
            return (struct.pack('>I', len(content)) + kind + content +
                    struct.pack('>I', zlib.crc32(kind + content)))
        png = (b'\x89PNG\r\n\x1a\n' +
               chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
               chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
        output = pathlib.Path(output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(png)
        return output.resolve()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('command', choices=[
        'sync-time', 'status', 'storage', 'display', 'page', 'timer', 'screen', 'sleep', 'wake',
        'wifi-setup', 'wifi-sync', 'wifi-forget', 'pwr-short', 'pwr-long', 'pwr-release',
        'boot-long', 'version-tap', 'menu-scroll', 'eclipse-close', 'evidence-start', 'evidence-export',
        'spectrum-start', 'cidr', 'rf', 'vlsm', 'dizzy', 'stopwatch-start',
        'stopwatch-lap', 'stopwatch-reset'])
    parser.add_argument('value', nargs='?')
    parser.add_argument('--port', default='COM4')
    args = parser.parse_args()
    command = args.command.upper().replace('-', '_')
    if args.command in ('page', 'timer', 'menu-scroll'):
        try:
            value = int(args.value)
        except (TypeError, ValueError):
            parser.error('page, timer y menu-scroll requieren un numero')
        bounds = {'page': (0, 16), 'timer': (1, 5999), 'menu-scroll': (-2000, 2000)}
        low, high = bounds[args.command]
        if not low <= value <= high:
            parser.error(f'{args.command}: {low}..{high}')
        command += f' {value}'
    if args.command == 'cidr':
        try:
            address = ipaddress.IPv4Interface(args.value)
        except (TypeError, ValueError):
            parser.error('cidr requiere IPv4/prefijo, por ejemplo 192.168.1.10/24')
        command += f' {address.ip}/{address.network.prefixlen}'
    if args.command in ('rf', 'vlsm'):
        if not args.value or len(args.value) > 80 or any(c not in '0123456789.,/-' for c in args.value):
            parser.error('rf/vlsm requieren hasta 80 caracteres numericos con punto, coma, / o -')
        command += ' ' + args.value
    if args.command == 'sync-time':
        offset = int(datetime.datetime.now().astimezone().utcoffset().total_seconds())
        command = f'TIME {int(time.time())} {offset}'
    watch = Watch(args.port)
    try:
        if args.command == 'screen':
            print(watch.screenshot(args.value or 'screen.png'))
        elif args.command == 'evidence-export':
            path, count = watch.evidence_export(args.value or 'evidence.jsonl')
            print(f'{path} ({count} records)')
        else:
            watch.send(command)
            prefix = {'sync-time': 'AURA_TIME ', 'status': 'AURA_STATUS ', 'storage': 'AURA_STORAGE ',
                      'display': 'AURA_DISPLAY ', 'wifi-setup': 'AURA_WIFI ',
                      'wifi-sync': 'AURA_WIFI ', 'wifi-forget': 'AURA_WIFI '}.get(args.command)
            if prefix:
                line = watch.response(prefix)
                print(line)
                if args.command == 'sync-time' and line != 'AURA_TIME ESP_OK':
                    raise RuntimeError('Clock synchronization failed: ' + line)
    finally:
        watch.close()

if __name__ == '__main__':
    main()
