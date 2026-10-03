"""Check invalidation while asleep and IMU reaction gating via real UI callbacks."""
import argparse
import json
import pathlib
import time
from watch import Watch
from scroll_benchmark import draw
from storage_check import storage


def run(watch):
    watch.send('WAKE'); watch.send('PAGE 3'); watch.send('BOOT_LONG')
    watch.send('SLEEP'); time.sleep(0.6)
    baseline = watch.display(); draw(watch,True)
    watch.send('ECLIPSE_CLOSE')  # Changes hidden screen styles and invalidates LVGL.
    for command in ('PAGE 3','DIZZY','MENU_SCROLL 240'):
        watch.send(command)
    watch.send('SCREEN')
    assert watch.response('AURA_ERROR ') == 'AURA_ERROR screen_asleep'
    time.sleep(3)
    asleep = watch.display(); stats = draw(watch)
    assert asleep['sleeping'] == '1' and asleep['eclipse'] == '0' and asleep['cpu_mhz'] == '80',asleep
    assert asleep['completed'] == baseline['completed'], 'Hidden invalidation resumed DMA'
    assert stats['refreshes'] == 0, stats
    assert int(asleep['uptime_ms']) > int(baseline['uptime_ms']), 'Unexpected reset'
    watch.send('WAKE'); time.sleep(0.4)
    awake = watch.display()
    assert awake['sleeping'] == '0' and int(awake['completed']) > int(asleep['completed'])
    watch.send('PAGE 3'); watch.send('BOOT_LONG'); watch.send('PAGE 15')
    watch.send('RF 915,10,-5'); watch.send('ENGINEER_KEY -1'); time.sleep(0.2)
    watch.send('DIZZY'); time.sleep(0.3)
    editor = watch.display(); assert editor['page'] == '15' and editor['eclipse'] == '1',editor
    watch.send('ENGINEER_KEY 17'); time.sleep(0.2)
    # Leave Evidence immediately; cancellation finishes current IO and releases SD.
    before = storage(watch)
    watch.send('PAGE 9'); watch.send('EVIDENCE_START'); watch.send('PAGE 0')
    deadline = time.monotonic()+20
    while watch.display()['evidence_busy'] == '1':
        if time.monotonic() > deadline: raise TimeoutError('Cancelled Evidence stuck')
        time.sleep(0.1)
    after = storage(watch)
    assert after['mounted'] == '0',after
    assert int(after['internal_records'])-int(before['internal_records']) in (0,1),after
    final = watch.display(); assert final['page'] == '0' and final['eclipse'] == '1'
    return {'result':'PASS','version':baseline['version'],'asleep_before':baseline,'asleep_after':asleep,
            'asleep_draw':stats,'awake':awake,'dizzy_editor':editor,'cancelled_evidence':after,
            'final':final,'read_retries':watch.read_retries,
            'scope':'USB LVGL callbacks and MCU counters, no physical touch/FPS/battery measurement'}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--port',default='COM4')
    parser.add_argument('--output',default='build/sleep-check.json'); args=parser.parse_args()
    watch=Watch(args.port)
    try:
        result=run(watch); pathlib.Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':result['result'],'asleep_refreshes':result['asleep_draw']['refreshes'],
                          'read_retries':result['read_retries']}))
    finally:watch.close()


if __name__ == '__main__':main()
