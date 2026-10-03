"""Verify the saved idle timeout on hardware without changing preferences."""
import argparse
import json
import pathlib
import time
from watch import Watch
from storage_check import preferences, storage
from features_check import status


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--port',default='COM4')
    parser.add_argument('--output',default='build/idle-check.json');args=parser.parse_args()
    watch=Watch(args.port)
    try:
        prefs=preferences(watch);timeout=int(prefs['idle_seconds'])
        watch.send('WAKE');watch.send('PAGE 0')
        before=watch.display();assert before['eclipse']=='1' and before['sleeping']=='0',before
        time.sleep(timeout+1.2)
        asleep=watch.display();assert asleep['sleeping']=='1' and asleep['cpu_mhz']=='80',asleep
        assert asleep['eclipse']=='1' and asleep['evidence_busy']=='0',asleep
        time.sleep(3)
        later=watch.display();assert later['completed']==asleep['completed'],'Idle sleep still drawing'
        assert int(later['uptime_ms'])>int(before['uptime_ms']),'Unexpected reset'
        assert status(watch)['radio']=='0' and storage(watch)['mounted']=='0'
        assert preferences(watch)==prefs,'Saved preferences changed'
        result={'result':'PASS','version':before['version'],'timeout_seconds':timeout,
                'awake':before,'asleep':asleep,'later':later,'radio':'0','mounted':'0',
                'preferences_preserved':prefs,'read_retries':watch.read_retries}
        pathlib.Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':'PASS','timeout_seconds':timeout,'cpu_mhz':later['cpu_mhz'],
                          'idle_dma':int(later['completed'])-int(asleep['completed'])}))
    finally:watch.close()


if __name__=='__main__':main()
