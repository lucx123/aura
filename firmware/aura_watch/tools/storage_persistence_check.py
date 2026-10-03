"""Compare a previous internal export after an intentional device reset/update."""
import argparse
import hashlib
import json
import pathlib
from watch import Watch
from storage_check import storage


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--port',default='COM4')
    parser.add_argument('--baseline',default='build/evidence-export.jsonl')
    parser.add_argument('--baseline-report',default='build/storage-check.json')
    parser.add_argument('--output',default='build/storage-persistence-check.json')
    args=parser.parse_args();baseline=pathlib.Path(args.baseline).read_bytes()
    report=json.loads(pathlib.Path(args.baseline_report).read_text())
    sequence=(report['storage'] if 'storage' in report else report['checks'][-1]['storage'])['internal_records']
    watch=Watch(args.port)
    try:
        after=storage(watch)
        assert after['internal_records']==sequence and after['internal_count']==str(len(baseline.splitlines())),after
        assert after['internal_verified']=='0', 'This check must run before any new record in the current boot'
        watch.send('WAKE');watch.send('PAGE 3');watch.send('BOOT_LONG')
        watch.wait_eclipse_ready()
        path,count=watch.evidence_export(pathlib.Path(args.output).with_suffix('.jsonl'))
        assert path.read_bytes()==baseline,'Persistent records changed after reset'
        result={'result':'PASS','version':watch.display()['version'],'storage':after,
                'export_records':count,'export_sha256':hashlib.sha256(baseline).hexdigest().upper(),
                'read_retries':watch.read_retries,'scope':'Internal NVS records preserved across intentional app update and hard reset'}
        pathlib.Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':'PASS','retained_records':count,'sequence':sequence}))
    finally:watch.close()


if __name__=='__main__':main()
