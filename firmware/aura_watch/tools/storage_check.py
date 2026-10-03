"""Verify manual Evidence, NTFS detection and bounded internal export on the MCU."""
import argparse
import json
import pathlib
import re
import time
from watch import Watch


def storage(watch):
    watch.send('STORAGE')
    return dict(item.split('=', 1) for item in watch.response('AURA_STORAGE ').split()[1:])


def preferences(watch):
    watch.send('STATUS')
    fields = dict(re.findall(r'([a-z_]+)=(.*?)(?= [a-z_]+=|$)', watch.response('AURA_STATUS ')))
    return {key: fields[key] for key in ('offset','brightness','theme','clock','configured','idle_seconds')}


def evidence(watch, timeout=20):
    before = storage(watch)
    watch.send('EVIDENCE_START')
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        display = watch.display()
        if display['evidence_busy'] == '0':
            after = storage(watch)
            assert after['mounted'] == '0', 'SD host was not released'
            sd_success = (int(after['records']) == int(before['records'])+1 and after['verified'] == '1')
            internal_success = (int(after['internal_records']) == int(before['internal_records'])+1 and
                                after['internal_verified'] == '1' and after['internal_error'] == 'ESP_OK')
            assert sd_success or internal_success, after
            return {'target': 'SD' if sd_success else 'INTERNAL', 'storage': after, 'display': display}
        time.sleep(0.1)
    raise TimeoutError('Evidence worker did not finish')


def denied_export(watch):
    watch.send('EVIDENCE_EXPORT')
    line = watch.response('AURA_EVIDENCE_END ')
    assert line == 'AURA_EVIDENCE_END error=ESP_ERR_INVALID_STATE count=0', line


def run(watch, output, repeats):
    # Let boot NTP finish so screenshots contain no configured network header.
    deadline = time.monotonic()+60
    while True:
        watch.send('STATUS')
        fields = dict(re.findall(r'([a-z_]+)=(.*?)(?= [a-z_]+=|$)', watch.response('AURA_STATUS ')))
        if fields['radio'] == '0': break
        if time.monotonic() > deadline: raise TimeoutError('Radio did not become idle')
        time.sleep(0.5)
    time.sleep(1.1)
    watch.send('WAKE'); watch.send('ECLIPSE_CLOSE')
    baseline = watch.display()
    assert baseline['version'] == '1.4.0-dev.6', baseline
    prefs = preferences(watch)
    denied_export(watch)
    watch.send('PAGE 3'); watch.send('BOOT_LONG'); watch.send('PAGE 9')
    prior_path, prior_count = watch.evidence_export(output/'evidence-before-check.jsonl')
    prior_records = [json.loads(line) for line in prior_path.read_text(encoding='utf-8').splitlines()]
    prior_sequence = int(storage(watch)['internal_records'])
    checks = [evidence(watch) for _ in range(repeats)]
    for check in checks:
        assert int(check['display']['uptime_ms']) > int(baseline['uptime_ms']), 'Unexpected reset'
        assert check['storage']['format'] == 'NTFS', 'Connected card format changed'
        assert check['storage']['bytes'] == '31914983424', 'Capacity changed'
        assert check['storage']['lba'] == '2048' and check['storage']['probe_error'] == 'ESP_OK'
        assert check['target'] == 'INTERNAL' and check['storage']['error'] != 'ESP_OK'
    watch.screenshot(output/'evidence.png')
    path, count = watch.evidence_export(output/'evidence-export.jsonl')
    records = [json.loads(line) for line in path.read_text().splitlines()]
    assert count == int(storage(watch)['internal_count']) <= 8
    assert int(storage(watch)['internal_records']) == prior_sequence + repeats
    retained = prior_records[-max(0,8-repeats):] if repeats < 8 else []
    assert records[:len(retained)] == retained, 'Ring changed previously retained records'
    assert all(record['event'] == 'system_check' and record['detail'] == 'local_manual_check' for record in records)
    assert preferences(watch) == prefs, 'Evidence changed saved preferences'
    watch.send('SLEEP'); time.sleep(0.4); denied_export(watch)
    watch.send('WAKE'); watch.send('PAGE 0')
    return {'result':'PASS','version':baseline['version'],'checks':checks,
            'export_records':count,'preferences_preserved':prefs,
            'prior_records':prior_count,'prior_sequence':prior_sequence,'prior_tail_preserved':True,
            'export_gated_without_mode_and_while_asleep':True,
            'final':watch.display(),'read_retries':watch.read_retries,
            'scope':'Actual NTFS card boot metadata and internal NVS readback; no SD formatting or file enumeration'}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--port',default='COM4')
    parser.add_argument('--output',default='build'); parser.add_argument('--repeats',type=int,default=3)
    args = parser.parse_args(); assert 1 <= args.repeats <= 10
    output = pathlib.Path(args.output); output.mkdir(parents=True,exist_ok=True)
    watch = Watch(args.port)
    try:
        result = run(watch,output,args.repeats)
        (output/'storage-check.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':result['result'],'targets':[c['target'] for c in result['checks']],
                          'records':result['export_records'],'read_retries':result['read_retries']}))
    finally: watch.close()


if __name__ == '__main__': main()
