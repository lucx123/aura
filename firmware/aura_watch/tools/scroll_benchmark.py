"""Compare scripted scroll and LVGL refresh-cycle timing on the actual MCU.

Measures LVGL cycles, not physical frame rate or touch-controller latency.
"""
import argparse
import json
import pathlib
import time
from watch import Watch
from features_check import status, wait_status


def draw(watch, reset=False):
    watch.send('DRAW_STATS_RESET' if reset else 'DRAW_STATS')
    line = watch.response('AURA_DRAW ')
    return {k: int(v) for k, v in (item.split('=', 1) for item in line.split()[1:])}


def run(watch):
    wait_status(watch, lambda s: s['radio'] == '0', 60)
    watch.send('WAKE')
    watch.send('ECLIPSE_CLOSE')
    results = []
    for page in (3, 6):
        if page == 6:
            watch.send('PAGE 3')
            watch.send('BOOT_LONG')
        watch.send(f'PAGE {page}')
        watch.send('MENU_SCROLL -2000')
        time.sleep(0.5)
        start = watch.display()
        draw(watch, True)
        begun = time.monotonic()
        for delta in [240, -240] * 12:
            watch.send(f'MENU_SCROLL {delta}')
            time.sleep(0.6)
        elapsed = time.monotonic() - begun
        stats = draw(watch)
        end = watch.display()
        assert end['page'] == str(page) and end['sleeping'] == '0', end
        assert int(end['uptime_ms']) > int(start['uptime_ms']), 'Unexpected reboot'
        assert stats['refreshes'] > 24, stats
        results.append({'page': page, 'elapsed_seconds': elapsed, 'stats': stats,
                        'cycle_mean_ms': stats['total_us'] / stats['refreshes'] / 1000,
                        'cycles_per_scripted_scroll': stats['refreshes'] / 24})
    watch.send('PAGE 0')
    return {'result': 'PASS', 'results': results, 'final': watch.display(),
            'read_retries': watch.read_retries,
            'method': '24 USB animated scrolls per menu; LVGL cycle timings, no physical FPS claim'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--output', default='build/scroll-benchmark.json')
    args = parser.parse_args()
    watch = Watch(args.port)
    try:
        result = run(watch)
        output = pathlib.Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(result['results']))
    finally:
        watch.close()


if __name__ == '__main__': main()
