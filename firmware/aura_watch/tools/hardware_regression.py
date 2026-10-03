"""Check transitions, Eclipse gesture, scrolling, sleep and timer wake.

Requires a connected watch. Leaves home awake. Radios/SD are checked separately.
"""
import argparse
import json
import pathlib
import time
from watch import Watch, project_version


def run(watch, expected):
    watch.send('SLEEP')
    time.sleep(0.6)
    watch.send('WAKE')
    time.sleep(0.6)
    watch.send('ECLIPSE_CLOSE')
    previous = watch.display()
    assert previous['version'] == expected, previous
    samples = [previous]
    for page in [1, 3, 4, 0, 2, 3, 5, 13, 3, 0] * 3:
        watch.send(f'PAGE {page}')
        time.sleep(0.25)
        current = watch.display()
        assert int(current['uptime_ms']) > int(previous['uptime_ms']), 'Watch rebooted'
        assert int(current['completed']) > int(previous['completed']), f'Stalled PAGE {page}'
        assert int(current['page']) == page, current
        assert current['eclipse'] == '0', 'Eclipse opened unexpectedly'
        samples.append(current)
        previous = current
    for page in (7, 15, 16):
        watch.send(f'PAGE {page}')
        current = watch.display()
        assert current['page'] == '0' and current['eclipse'] == '0', 'Hidden page opened without gesture'
    watch.send('PAGE 3')
    time.sleep(0.3)
    scroll_samples = []
    for pixels in [240, -240] * 6:
        before = watch.display()
        watch.send(f'MENU_SCROLL {pixels}')
        time.sleep(0.35)
        after = watch.display()
        assert after['page'] == '3' and after['eclipse'] == '0', after
        assert int(after['completed']) > int(before['completed']), 'Scroll did not draw'
        assert int(after['uptime_ms']) > int(before['uptime_ms']), 'Scroll caused reboot'
        assert int(after['menu_y']) != int(before['menu_y']), 'Menu offset did not change'
        scroll_samples.append({'pixels': pixels, 'before': before, 'after': after})
    time.sleep(0.5)
    for _ in range(6):
        watch.send('VERSION_TAP')
    assert watch.display()['eclipse'] == '0', 'Six taps opened Eclipse'
    time.sleep(5.2)
    watch.send('VERSION_TAP')
    assert watch.display()['eclipse'] == '0', 'Expired gesture opened Eclipse'
    for _ in range(6):
        watch.send('VERSION_TAP')
    entering = watch.display()
    assert entering['page'] == '0' and entering['eclipse'] == '1' and entering['eclipse_intro'] == '1', entering
    entered = watch.wait_eclipse_ready()
    assert entered['page'] == '0' and entered['eclipse'] == '1', 'Seven taps failed'
    for page in (7, 8, 9, 10, 11, 12, 15, 16, 6):
        watch.send(f'PAGE {page}')
        time.sleep(0.25)
        assert watch.display()['page'] == str(page), f'Eclipse PAGE {page} failed'
    watch.send('ECLIPSE_CLOSE')
    closed = watch.display()
    assert closed['eclipse'] == '0' and closed['page'] == '0', 'Close did not end Eclipse'
    watch.send('PAGE 3')
    watch.send('BOOT_LONG')
    assert watch.wait_eclipse_ready()['eclipse'] == '1', 'BOOT shortcut failed'
    watch.send('SLEEP')
    time.sleep(1)
    start = watch.display()
    time.sleep(3)
    end = watch.display()
    assert end['sleeping'] == '1' and end['eclipse'] == '1', end
    assert end['completed'] == start['completed'], 'Display still drawing while asleep'
    assert end['cpu_mhz'] == '80', 'CPU did not scale down'
    watch.send('PWR_SHORT')
    time.sleep(0.3)
    awake = watch.display()
    assert awake['sleeping'] == '0' and awake['cpu_mhz'] == '240', awake
    assert awake['eclipse'] == '1', 'Wake ended Eclipse'
    watch.send('ECLIPSE_CLOSE')
    watch.send('PWR_SHORT')
    assert watch.display()['page'] == '3', 'PWR did not open menu'
    watch.send('PWR_SHORT')
    assert watch.display()['page'] == '0', 'PWR did not return home'
    watch.send('PWR_LONG')
    watch.send('PWR_RELEASE')
    assert watch.display()['sleeping'] == '1', 'Medium PWR gesture failed'
    watch.send('WAKE')
    watch.send('TIMER 3')
    watch.send('SLEEP')
    time.sleep(4.2)
    timer = watch.display()
    assert timer['sleeping'] == '0' and timer['page'] == '2', 'Timer did not wake watch'
    assert int(timer['uptime_ms']) > int(end['uptime_ms']), 'Watch rebooted during sleep'
    watch.send('PAGE 0')
    final = watch.display()
    return {'version': expected, 'transitions': 30, 'samples': samples,
            'scrolls': scroll_samples, 'eclipse_entered': entered, 'eclipse_closed': closed,
            'sleep_start': start, 'sleep_end': end, 'timer_wake': timer, 'final': final,
            'result': 'PASS', 'read_retries': watch.read_retries,
            'visual_validation': 'requires inspection of physical panel'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--expected-version', default=project_version())
    parser.add_argument('--output', default='build/hardware-regression.json')
    args = parser.parse_args()
    watch = Watch(args.port)
    try:
        result = run(watch, args.expected_version)
        output = pathlib.Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print('PASS: 30 transitions, 12 scrolls, seven taps, Eclipse close, idle DMA, CPU, PWR and timer.')
        print(output.resolve())
    finally:
        watch.close()


if __name__ == '__main__':
    main()
