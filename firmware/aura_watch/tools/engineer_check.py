"""Check offline engineering apps and gating through USB on the real MCU.

LVGL snapshots validate rendering; touch-controller and hit-testing remain manual.
"""
import argparse
import json
import pathlib
import time
from watch import Watch
from features_check import status, wait_status
from eclipse_navigation_check import page_body_hash


def run(watch, output):
    checks = []
    wait_status(watch, lambda s: s['radio'] == '0', 60)
    watch.send('WAKE')
    watch.send('ECLIPSE_CLOSE')

    def expect(page, mode=True, sleeping=False):
        current = watch.display()
        assert current['version'] == '1.4.0-dev.6', current
        assert current['page'] == str(page) and current['eclipse'] == str(int(mode)), current
        assert current['sleeping'] == str(int(sleeping)), current
        assert status(watch)['radio'] == '0', 'Offline app enabled radio'
        return current

    def capture(page, name):
        watch.send(f'PAGE {page}')
        time.sleep(0.3)
        expect(page)
        path = output / name
        watch.screenshot(path)
        return page_body_hash(path)

    def key(index):
        watch.send(f'ENGINEER_KEY {index}')
        time.sleep(0.08)

    def type_input(text):
        keys = '123\b456X789-.0/,'
        key(7)  # Borrar
        for char in text: key(keys.index(char))

    for page in (15, 16):
        watch.send(f'PAGE {page}')
        expect(0, False)
    checks.append('RF and VLSM pages blocked without Eclipse')
    watch.send('PAGE 3')
    watch.send('BOOT_LONG')
    expect(6)
    watch.send('MENU_SCROLL -2000')
    time.sleep(0.6)
    watch.screenshot(output / 'eclipse.png')
    top = watch.display()
    watch.send('MENU_SCROLL 2000')
    time.sleep(0.6)
    bottom = expect(6)
    assert int(bottom['menu_y']) > int(top['menu_y']), bottom
    watch.screenshot(output / 'eclipse-bottom.png')
    watch.send('MENU_SCROLL -2000')
    time.sleep(0.6)
    checks.append('eight-tool Eclipse menu scrolls to explicit exit')

    watch.send('PAGE 15')
    watch.send('RF 2400,100,-20')
    rf_hash = capture(15, 'rf.png')
    watch.send('RF 0,100,-20')
    bad_rf = capture(15, 'rf-invalid.png')
    assert rf_hash != bad_rf, 'RF invalid input did not update output'
    watch.send('RF 2400,100,-20')
    assert capture(15, 'rf-check.png') == rf_hash
    checks.append('RF valid, invalid and restored output render with radio off')

    key(-1)
    time.sleep(0.2)
    watch.screenshot(output / 'engineer-keyboard.png')
    type_input('915,10,-5')
    key(16)  # Aplicar in the LVGL task
    edited_rf = capture(15, 'rf-keypad-check.png')
    assert edited_rf != rf_hash, 'Keyboard Apply did not update RF'
    watch.send('RF 915,10,-5')
    assert capture(15, 'rf-check.png') == edited_rf, 'Keyboard and direct RF calculation disagree'
    key(-1)
    type_input('1,1,0')
    key(17)  # Cancelar
    assert capture(15, 'rf-check.png') == edited_rf, 'Cancel did not restore RF input'
    watch.send('RF 2400,100,-20')
    assert capture(15, 'rf-check.png') == rf_hash
    checks.append('RF keyboard applies in LVGL task and Cancel restores prior input')

    watch.send('PAGE 16')
    watch.send('VLSM 192.168.1.0/24,50,20,10')
    vlsm_hash = capture(16, 'vlsm.png')
    watch.send('VLSM 192.168.1.0/30,50,20,10')
    assert capture(16, 'vlsm-invalid.png') != vlsm_hash
    watch.send('VLSM 192.168.1.0/24,50,20,10')
    assert capture(16, 'vlsm-check.png') == vlsm_hash
    checks.append('VLSM allocation and no-space error render with radio off')
    key(-1)
    type_input('10.0.0.0/24,100,20')
    key(16)
    edited_vlsm = capture(16, 'vlsm-keypad-check.png')
    assert edited_vlsm != vlsm_hash, 'Keyboard Apply did not update VLSM'
    watch.send('VLSM 10.0.0.0/24,100,20')
    assert capture(16, 'vlsm-check.png') == edited_vlsm
    key(-1)
    type_input('1')
    watch.send('PAGE 15')  # Leaving restores the last applied VLSM input.
    assert capture(16, 'vlsm-check.png') == edited_vlsm
    watch.send('VLSM 192.168.1.0/24,50,20,10')
    assert capture(16, 'vlsm-check.png') == vlsm_hash
    checks.append('shared VLSM keyboard applies in LVGL task and navigation cancels edit')
    watch.send('RF 100,1,0')
    assert capture(15, 'rf-check.png') == rf_hash, 'Hidden RF input accepted'
    watch.send('VLSM 10.0.0.0/8,500')
    assert capture(16, 'vlsm-check.png') == vlsm_hash, 'Hidden VLSM input accepted'
    checks.append('USB inputs ignored while the target app is hidden')

    watch.send('SLEEP')
    time.sleep(0.5)
    asleep = expect(0, sleeping=True)
    watch.send('PAGE 15')
    watch.send('RF 100,1,0')
    watch.send('PAGE 16')
    watch.send('VLSM 10.0.0.0/8,500')
    time.sleep(1)
    end = expect(0, sleeping=True)
    assert end['completed'] == asleep['completed'] and end['cpu_mhz'] == '80', end
    watch.send('WAKE')
    assert capture(15, 'rf-check.png') == rf_hash, 'Sleeping RF input accepted'
    assert capture(16, 'vlsm-check.png') == vlsm_hash, 'Sleeping VLSM input accepted'
    checks.append('sleep stops DMA, preserves mode and rejects calculator input')

    watch.send('PAGE 15')
    time.sleep(0.45)
    for _ in range(3):
        watch.send('TAP 205 450 0')
        time.sleep(0.07)
    time.sleep(0.4)
    expect(0, sleeping=True)
    for _ in range(2):
        watch.send('TAP 205 450 0')
        time.sleep(0.07)
    expect(0)
    checks.append('RF triple lock and double wake retain Eclipse')
    assert capture(15, 'rf-check.png') == rf_hash
    assert capture(16, 'vlsm-check.png') == vlsm_hash
    checks.append('calculator results retained across lock and wake')

    watch.send('ECLIPSE_CLOSE')
    for page, command in ((15, 'RF 2400,100,-20'), (16, 'VLSM 192.168.1.0/24,50')):
        watch.send(f'PAGE {page}')
        watch.send(command)
        expect(0, False)
    checks.append('explicit exit blocks both apps and their inputs')
    return {'result': 'PASS', 'checks': checks, 'menu_top': top, 'menu_bottom': bottom,
            'sleep': end, 'rf_body_sha256': rf_hash, 'vlsm_body_sha256': vlsm_hash,
            'final': watch.display(), 'read_retries': watch.read_retries,
            'method': 'USB callbacks and LVGL screenshots; physical touch pending'}


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
        (output / 'engineer-check.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(f"PASS: {len(result['checks'])} engineering app checks.")
    finally:
        watch.close()


if __name__ == '__main__':
    main()
