"""Check persistent Eclipse mode and paused tools on the real MCU over USB."""
import argparse
import hashlib
import json
import pathlib
import struct
import time
import zlib
from watch import Watch, project_version
from features_check import status, wait_status


def page_body_hash(path):
    """Compare LVGL page pixels while excluding the live status header."""
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, compressed = 8, bytearray()
    while offset < len(data):
        length = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]
        content = data[offset+8:offset+8+length]
        if kind == b'IHDR':
            width, height, depth, color, *_ = struct.unpack('>IIBBBBB', content)
            assert (width, height, depth, color) == (410, 502, 8, 2)
        if kind == b'IDAT': compressed.extend(content)
        offset += length + 12
    rows = zlib.decompress(compressed)
    stride = 1 + width * 3
    assert len(rows) == stride * height
    assert all(rows[y * stride] == 0 for y in range(height))
    return hashlib.sha256(rows[62*stride:]).hexdigest()


def run(watch, output):
    checks = []
    wait_status(watch, lambda s: s['radio'] == '0', 60)
    watch.send('WAKE')
    watch.send('ECLIPSE_CLOSE')
    watch.send('PAGE 3')
    watch.send('BOOT_LONG')
    watch.wait_eclipse_ready()

    def expect(page, mode=True):
        current = watch.display()
        assert current['version'] == project_version(), current
        assert current['page'] == str(page) and current['eclipse'] == str(int(mode)), current
        return current

    expect(0)
    time.sleep(0.4)
    watch.screenshot(output / 'home-hacker.png')
    watch.send('PWR_SHORT')
    expect(6)
    watch.send('PWR_SHORT')
    expect(0)
    watch.send('PWR_SHORT')
    expect(6)
    checks.append('PWR closes menu and reopens Eclipse without ending mode')
    for page in (1, 2, 3, 4, 5, 13, 14, 0):
        watch.send(f'PAGE {page}')
        expect(page)
    checks.append('all basic apps retain mode')

    before = status(watch)
    watch.send('SPECTRUM_START')
    watch.send('EVIDENCE_START')
    time.sleep(0.6)
    rejected = status(watch)
    assert rejected['radio'] == '0' and rejected['sd_error'] == before['sd_error'], rejected
    checks.append('home rejects hidden scan and Evidence starts')

    watch.send('PWR_SHORT')
    expect(6)
    watch.send('PAGE 7')
    watch.send('SPECTRUM_START')
    time.sleep(0.5)
    finished = wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] in ('0', '5'), 20)
    assert finished['wifi'] == '0', finished
    time.sleep(1.1)
    baseline_path = output / 'spectrum-cache-before.png'
    watch.screenshot(baseline_path)
    cached_hash = page_body_hash(baseline_path)

    def cache_unchanged(name):
        watch.send('PAGE 7')
        time.sleep(1.1)
        path = output / 'spectrum-cache-after.png'
        watch.screenshot(path)
        assert page_body_hash(path) == cached_hash, name
        assert status(watch)['radio'] == '0'
        checks.append(name)

    watch.send('PAGE 0')
    expect(0)
    watch.send('PWR_SHORT')
    expect(6)
    cache_unchanged('home navigation retains completed scan')

    watch.send('SPECTRUM_START')
    wait_status(watch, lambda s: s['wifi'] == '6', 3)
    watch.send('PAGE 0')
    cancelled = wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] != '6', 3)
    expect(0)
    cache_unchanged('cancelled replacement scan retains completed cache')

    for page in (15, 16):
        watch.send('SPECTRUM_START')
        wait_status(watch, lambda s: s['wifi'] == '6', 3)
        watch.send(f'PAGE {page}')
        wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] != '6', 3)
        expect(page)
        cache_unchanged(f'offline PAGE {page} cancels replacement scan and retains cache')

    watch.send('PAGE 3')
    watch.send('BOOT_LONG')
    watch.wait_eclipse_ready()
    cache_unchanged('BOOT reentry retains cache')
    watch.send('PAGE 3')
    for _ in range(7): watch.send('VERSION_TAP')
    watch.wait_eclipse_ready()
    cache_unchanged('seven version taps retain existing session')

    watch.send('SLEEP')
    time.sleep(0.5)
    sleeping = expect(0)
    assert sleeping['sleeping'] == '1' and sleeping['cpu_mhz'] == '80', sleeping
    completed = sleeping['completed']
    time.sleep(1)
    assert expect(0)['completed'] == completed
    watch.send('PWR_SHORT')
    expect(0)
    watch.send('PWR_SHORT')
    expect(6)
    cache_unchanged('sleep and PWR wake retain mode and cached scan')

    watch.send('TIMER 3')
    watch.send('SLEEP')
    time.sleep(4.2)
    timer = expect(2)
    assert timer['sleeping'] == '0'
    checks.append('timer wakes without ending Eclipse')
    watch.send('PWR_SHORT')
    expect(6)
    watch.screenshot(output / 'eclipse.png')
    watch.send('ECLIPSE_CLOSE')
    expect(0, False)
    for page in (7, 15, 16):
        watch.send(f'PAGE {page}')
        expect(0, False)
    checks.append('explicit exit closes mode and blocks tools')
    return {'result': 'PASS', 'checks': checks, 'cache_body_sha256': cached_hash,
            'cancelled_scan': cancelled, 'sleep': sleeping, 'timer': timer,
            'final': watch.display(), 'read_retries': watch.read_retries,
            'method': 'USB callbacks and LVGL snapshots; physical touch pending'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--output', default='build')
    args = parser.parse_args()
    output = pathlib.Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    watch = Watch(args.port)
    try:
        result = run(watch, output)
        (output / 'eclipse-navigation-check.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(f"PASS: {len(result['checks'])} Eclipse navigation checks.")
    finally: watch.close()


if __name__ == '__main__': main()
