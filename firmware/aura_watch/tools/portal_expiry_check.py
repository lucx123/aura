"""Check the real five-minute portal deadline without changing credentials."""
import argparse
import json
import pathlib
import time
from features_check import status, wait_status
from watch import Watch


def run(watch):
    before = status(watch)
    assert before['radio'] == '0', 'Run while Wi-Fi is idle'
    started = time.monotonic()
    watch.send('WIFI_SETUP')
    opened = wait_status(watch, lambda s: s['wifi'] == '4' and s['radio'] == '1', 10)
    print('Portal opened; repeating setup at 270 seconds.', flush=True)
    repeated = False
    while True:
        elapsed = time.monotonic() - started
        current = status(watch)
        if current['radio'] == '0':
            assert repeated, f'Portal closed early after {elapsed:.1f}s'
            assert 299 <= elapsed <= 310, f'Unexpected deadline: {elapsed:.1f}s'
            assert current['wifi'] == '0', current
            break
        assert elapsed < 310, 'Repeating setup extended the portal deadline'
        if not repeated and elapsed >= 270:
            watch.send('WIFI_SETUP')
            repeated = True
            print('Repeated setup; checking original deadline.', flush=True)
        time.sleep(2 if elapsed >= 268 else 5)
    result = {'result': 'PASS', 'elapsed_seconds': elapsed,
              'repeat_after_seconds': 270, 'opened': opened, 'expired': current}
    assert current['configured'] == before['configured'], 'Configuration changed'
    if before['configured'] == '1':
        watch.send('WIFI_SYNC')
        result['restored_sync'] = wait_status(watch, lambda s: s['radio'] == '0' and
            s['wifi'] in ('3', '5'), 60)
    watch.send('WAKE')
    watch.send('PAGE 0')
    result['final'] = watch.display()
    result['read_retries'] = watch.read_retries
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--output', default='build/portal-expiry-check.json')
    args = parser.parse_args()
    watch = Watch(args.port)
    try:
        result = run(watch)
        output = pathlib.Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(f"PASS: portal expired after {result['elapsed_seconds']:.2f}s")
    finally:
        watch.close()


if __name__ == '__main__':
    main()
