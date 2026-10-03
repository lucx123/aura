"""Check saved Wi-Fi/NTP, Spectrum and Evidence using real firmware callbacks."""
import argparse
import json
import pathlib
import re
import time
from watch import Watch
from storage_check import evidence


def status(watch):
    watch.send('STATUS')
    line = watch.response('AURA_STATUS ')
    fields = dict(re.findall(r'([a-z_]+)=(.*?)(?= [a-z_]+=|$)', line))
    # Reports retain diagnostics without listing the saved network or nearby SSIDs.
    return {k: fields[k] for k in ('valid', 'rtc', 'configured', 'wifi', 'last_sync',
            'reason', 'radio', 'imu', 'sd', 'sd_error', 'sd_bytes')}


def wait_status(watch, predicate, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        current = status(watch)
        if predicate(current): return current
        time.sleep(0.5)
    raise TimeoutError(current)


def run(watch, output):
    watch.send('WAKE')
    before = status(watch)
    result = {'baseline': before}
    if before['configured'] == '1':
        watch.send('WIFI_SYNC')
        synced = wait_status(watch, lambda s: s['radio'] == '0' and
            s['wifi'] == '3' and int(s['last_sync']) > int(before['last_sync']), 60)
        assert synced['valid'] == '1' and synced['rtc'] == '1', synced
        result['ntp'] = {'result': 'PASS', 'status': synced}
    else:
        result['ntp'] = {'result': 'SKIP', 'reason': 'No saved network'}
    watch.send('WAKE')
    watch.send('PAGE 3')
    time.sleep(0.6)
    watch.screenshot(output / 'menu-top.png')
    watch.send('MENU_SCROLL 400')
    time.sleep(0.6)
    watch.screenshot(output / 'menu-bottom.png')
    for _ in range(7): watch.send('VERSION_TAP')
    watch.wait_eclipse_ready()
    assert watch.display()['eclipse'] == '1'
    watch.send('PAGE 6')
    watch.screenshot(output / 'eclipse.png')
    watch.send('PAGE 7')
    watch.send('SPECTRUM_START')
    time.sleep(0.5)
    scanned = wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] in ('0', '5'), 20)
    assert scanned['wifi'] == '0', scanned
    time.sleep(1.1)
    watch.screenshot(output / 'spectrum.png')
    result['spectrum'] = {'result': 'PASS', 'status': scanned}
    for page, name in ((10, 'channels'), (11, 'wifi-audit')):
        watch.send(f'PAGE {page}')
        time.sleep(1.1)
        watch.screenshot(output / f'{name}.png')
        assert status(watch)['radio'] == '0', 'Viewing cached data enabled radio'
    watch.send('PAGE 12')
    watch.send('CIDR 10.12.34.56/27')
    time.sleep(0.4)
    watch.screenshot(output / 'cidr.png')
    assert status(watch)['radio'] == '0', 'Offline calculator enabled radio'
    result['network_apps'] = {'result': 'PASS', 'pages': [10, 11, 12], 'radio': '0'}

    # Leave the tools workspace; mode persists while scan activity stops.
    watch.send('PAGE 7')
    watch.send('SPECTRUM_START')
    time.sleep(0.2)
    cancellation_started = time.monotonic()
    watch.send('PAGE 13')
    cancelled = wait_status(watch, lambda s: s['radio'] == '0' and s['wifi'] == '0', 3)
    assert watch.display()['eclipse'] == '1', 'Basic app incorrectly ended Eclipse'
    result['scan_cancel'] = {'result': 'PASS', 'seconds': time.monotonic()-cancellation_started,
                             'status': cancelled}
    watch.send('PAGE 3')
    watch.send('BOOT_LONG')
    watch.wait_eclipse_ready()
    watch.send('PAGE 9')
    checked = evidence(watch)
    time.sleep(1.1)
    watch.screenshot(output / 'evidence.png')
    result['evidence'] = {'result': 'PASS', **checked}
    watch.send('ECLIPSE_CLOSE')
    watch.send('PAGE 13')
    time.sleep(0.4)
    watch.screenshot(output / 'battery.png')
    watch.send('PAGE 14')
    time.sleep(0.4)
    watch.screenshot(output / 'flashlight.png')
    time.sleep(30.5)
    flashlight = watch.display()
    assert flashlight['page'] != '14', 'Flashlight did not end within 30 seconds'
    result['flashlight_timeout'] = {'result': 'PASS', 'display': flashlight}
    watch.send('WAKE')
    watch.send('PAGE 0')
    time.sleep(0.4)
    watch.screenshot(output / 'home.png')
    result['final'] = watch.display()
    result['read_retries'] = watch.read_retries
    return result


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
        (output / 'features-check.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(json.dumps({k: v['result'] for k,v in result.items() if isinstance(v,dict) and 'result' in v}))
        print(output.resolve())
    finally: watch.close()


if __name__ == '__main__': main()
