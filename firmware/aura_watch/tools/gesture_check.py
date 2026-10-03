"""Exercise triple lock and double wake through LVGL events on the real MCU.

These diagnostic events bypass physical touch and hit testing.
"""
import argparse
import json
import pathlib
import time
from watch import Watch, project_version
from features_check import status, wait_status


def run(watch):
    checks = []

    def awake(page=0):
        watch.send('WAKE')
        watch.send(f'PAGE {page}')
        time.sleep(0.45)

    def tap(x=205, y=160, target=0):
        watch.send(f'TAP {x} {y} {target}')
        time.sleep(0.07)

    def expect(sleeping, name):
        current = watch.display()
        assert current['sleeping'] == str(int(sleeping)), (name, current)
        assert current['cpu_mhz'] == ('80' if sleeping else '240'), (name, current)
        checks.append(name)
        return current

    wait_status(watch, lambda s: s['radio'] == '0', 60)
    watch.send('ECLIPSE_CLOSE')
    awake()
    assert watch.display()['version'] == project_version()
    tap(); tap()
    expect(False, 'two taps stay awake')
    tap()
    expect(True, 'third tap locks')
    tap(); tap()
    expect(True, 'extra immediate taps do not wake')
    time.sleep(0.45)
    tap()
    expect(True, 'single tap does not wake')
    tap()
    expect(False, 'double tap wakes')
    tap()
    expect(False, 'third tap after wake does not relock')

    awake()
    tap(target=3); tap(target=3); tap(target=3)
    expect(True, 'face triple tap locks')

    awake()
    tap(); tap(); time.sleep(0.8); tap()
    expect(False, 'expired sequence resets')
    tap(); tap()
    expect(True, 'new sequence can lock')

    awake()
    tap(x=100); tap(x=160); tap(x=160)
    expect(False, 'distant tap resets')
    tap(x=160)
    expect(True, 'nearby sequence locks')

    awake()
    tap(); tap(); watch.send('PAGE 1'); tap()
    expect(False, 'page change resets')
    tap(); tap()
    expect(False, 'clock background triple stays awake')

    for page in (1, 2, 3, 4, 5, 13, 14):
        awake(page)
        tap(); tap(); tap()
        current = expect(False, f'normal PAGE {page} ignores triple lock')
        assert current['page'] == str(page), current

    awake(3)
    baseline = status(watch)
    tap(); tap(); tap(target=1); tap(); tap()
    expect(False, 'control tap cancels background sequence')
    tap(target=1); tap(target=1); tap(target=1)
    expect(False, 'control triple does not lock')
    assert status(watch)['configured'] == baseline['configured']

    awake()
    tap(); tap(); watch.send('DRAG 205 160 245 160'); tap(); tap()
    expect(False, 'drag cancels tap sequence')
    tap()
    expect(True, 'fresh taps after drag lock')

    awake(3)
    for _ in range(3): tap(y=440, target=2)
    expect(False, 'version third tap does not lock')
    assert watch.display()['eclipse'] == '0'
    for _ in range(4): tap(y=440, target=2)
    entered = watch.display()
    assert entered['eclipse'] == '1' and entered['page'] == '0' and entered['eclipse_intro'] == '1', entered
    watch.wait_eclipse_ready()
    checks.append('seven version taps still enter Eclipse')

    for page in (3, 6, 7, 8, 9, 10, 11, 12, 15, 16):
        awake(page)
        tap(); tap(); tap()
        current = expect(False, f'Eclipse PAGE {page} ignores triple lock')
        assert current['page'] == str(page) and current['eclipse'] == '1', current

    watch.send('PAGE 7')
    watch.send('SPECTRUM_START')
    tap(); tap(); tap()
    expect(False, 'Spectrum triple stays awake during scan')
    watch.send('PAGE 0')
    time.sleep(0.45)
    tap(); tap(); tap()
    locked = expect(True, 'Eclipse home triple locks')
    assert locked['eclipse'] == '1', locked
    cancelled = wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] != '6', 3)
    checks.append('Eclipse scan radio is off after lock')

    time.sleep(0.45)
    tap(); tap()
    assert watch.display()['eclipse'] == '1', 'Double wake ended Eclipse'
    checks.append('double wake retains Eclipse')

    awake()
    watch.send('PWR_LONG')
    tap(); tap(); tap()
    expect(False, 'PWR hold ignores triple tap')
    watch.send('PWR_RELEASE')
    expect(True, 'PWR release retains its sleep action')
    awake()
    watch.send('ECLIPSE_CLOSE')
    return {'result': 'PASS', 'checks': checks, 'scan_cancelled': cancelled,
            'final': watch.display(), 'read_retries': watch.read_retries,
            'method': 'LVGL events injected over USB; physical touch and hit testing pending'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--output', default='build/gesture-check.json')
    args = parser.parse_args()
    watch = Watch(args.port)
    try:
        result = run(watch)
        output = pathlib.Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(f"PASS: {len(result['checks'])} gesture checks; physical touch pending.")
    finally:
        watch.close()


if __name__ == '__main__':
    main()
