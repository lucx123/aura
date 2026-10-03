"""Reproduce menu reopening, Eclipse entry and residual taps on the actual MCU.

USB events exercise LVGL callbacks; physical touch and hit testing remain manual.
"""
import argparse
import json
import pathlib
import time
from watch import Watch, project_version
from features_check import wait_status


def run(watch, output):
    checks, samples = [], []
    wait_status(watch, lambda s: s['radio'] == '0', 60)

    def current():
        value = watch.display()
        assert value['version'] == project_version(), value
        samples.append(value)
        return value

    def wait_stage(stage, timeout=6):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = current()
            assert value['page'] == '0' and value['eclipse'] == '1' and value['sleeping'] == '0', value
            if value['eclipse_intro'] == str(stage): return value
            time.sleep(.05)
        raise TimeoutError(f'Eclipse did not reach stage {stage}: {value}')

    def tap(target=0):
        watch.send(f'TAP 205 440 {target}')
        time.sleep(.07)

    def begin():
        watch.send('WAKE'); watch.send('ECLIPSE_CLOSE'); watch.send('PAGE 3')
        time.sleep(.45)
        watch.send('MENU_SCROLL 2000'); time.sleep(.55)
        assert int(current()['menu_y']) > 0
        for _ in range(6): tap(2)
        assert current()['eclipse'] == '0', 'Six taps opened Eclipse'
        tap(2)
        value = current()
        assert value['page'] == '0' and value['eclipse_intro'] == '1' and value['eclipse'] == '1', value
        return value

    watch.send('WAKE'); watch.send('ECLIPSE_CLOSE'); watch.send('PAGE 3')
    time.sleep(.45)
    watch.send('MENU_SCROLL 2000'); time.sleep(.55)
    assert int(current()['menu_y']) > 0
    watch.send('PWR_SHORT'); watch.send('PWR_SHORT')
    assert current()['menu_y'] == '0', 'Aura menu retained its previous offset'
    checks.append('Aura menu reopens at the top through PWR')
    watch.send('MENU_SCROLL 2000'); time.sleep(.03)
    watch.send('PAGE 0'); watch.send('PAGE 3'); time.sleep(.7)
    assert current()['menu_y'] == '0', 'Old Aura scroll animation resumed'
    checks.append('Aura reopening cancels an in-flight scroll')

    question = begin()
    watch.screenshot(output / 'eclipse-question.png')
    for command in ('PWR_SHORT', 'PAGE 6', 'MENU_SCROLL 1000', 'VERSION_TAP', 'BOOT_LONG'):
        watch.send(command)
    tap(); tap(); tap()
    guarded = current()
    assert guarded['page'] == '0' and guarded['eclipse'] == '1' and guarded['sleeping'] == '0', guarded
    checks.append('extra version/background taps and navigation cannot escape the intro')
    confirmation = wait_stage(2)
    assert int(confirmation['uptime_ms']) - int(question['uptime_ms']) >= 1250, confirmation
    watch.screenshot(output / 'eclipse-confirmation.png')
    home = wait_stage(0)
    # A screenshot transfer can delay the first observation of stage 2.
    # Check total readable duration from the observed question instead.
    assert int(home['uptime_ms']) - int(question['uptime_ms']) >= 5150, home
    time.sleep(.5)
    watch.screenshot(output / 'eclipse-home.png')
    checks.append('question and four-second confirmation finish automatically on Eclipse home')

    begin()
    tap(4)
    assert current()['eclipse_intro'] == '1', 'OK bypassed the initial question'
    checks.append('OK is inactive during the initial question')
    manual_confirmation = wait_stage(2)
    time.sleep(.1)
    tap(4)
    manual_home = current()
    assert manual_home['eclipse_intro'] == '0' and manual_home['eclipse'] == '1', manual_home
    assert int(manual_home['uptime_ms']) - int(manual_confirmation['uptime_ms']) < 1000, manual_home
    checks.append('fresh OK advances to Eclipse home without waiting for the automatic timeout')
    tap(); tap(); tap()
    assert current()['sleeping'] == '0', 'Extra taps after OK locked home'
    checks.append('OK completion retains the guard against residual home taps')

    watch.send('PWR_SHORT')
    assert current()['page'] == '6' and current()['menu_y'] == '0'
    watch.send('MENU_SCROLL 2000'); time.sleep(.55)
    assert int(current()['menu_y']) > 0
    watch.send('PWR_SHORT'); watch.send('PWR_SHORT')
    assert current()['page'] == '6' and current()['menu_y'] == '0'
    checks.append('Eclipse menu reopens at the top through PWR')
    watch.send('MENU_SCROLL 2000'); time.sleep(.03)
    watch.send('PAGE 7'); watch.send('PWR_SHORT'); time.sleep(.7)
    assert current()['page'] == '6' and current()['menu_y'] == '0'
    checks.append('Eclipse tool return cancels an in-flight menu scroll')

    begin()
    spam_transition_deadline = time.monotonic() + 3
    while current()['eclipse_intro'] == '1':
        if time.monotonic() > spam_transition_deadline:
            raise TimeoutError('Question did not finish during repeated taps')
        tap()
    tap(4)
    assert current()['eclipse_intro'] == '2', 'Entry taps accidentally acknowledged OK'
    checks.append('OK ignores entry taps until touch input has been quiet')
    spam_end = time.monotonic() + 4.5
    while time.monotonic() < spam_end: tap()
    assert current()['eclipse_intro'] == '2', 'Intro vanished while taps continued'
    time.sleep(.25)
    assert current()['eclipse_intro'] == '2', 'Intro did not wait for quiet touch input'
    checks.append('continued tapping holds the confirmation until touch input is quiet')
    wait_stage(0)
    tap(); tap(); tap()
    assert current()['sleeping'] == '0', 'Residual intro taps locked home'
    checks.append('final guard absorbs residual taps on home')
    time.sleep(.5)
    tap(); tap(); tap()
    assert current()['sleeping'] == '1' and current()['eclipse'] == '1'
    time.sleep(.45)
    tap(); tap()
    assert current()['sleeping'] == '0' and current()['eclipse'] == '1'
    checks.append('fresh home triple locks and double wake retains Eclipse')

    begin(); watch.send('SLEEP')
    asleep = current()
    assert asleep['sleeping'] == '1' and asleep['eclipse'] == '1' and asleep['eclipse_intro'] == '0', asleep
    completed = asleep['completed']
    time.sleep(.6)
    assert current()['completed'] == completed, 'Cancelled intro kept drawing while asleep'
    watch.send('WAKE'); time.sleep(.5)
    assert current()['page'] == '0' and current()['eclipse_intro'] == '0'
    checks.append('sleep cancels intro, retains mode and stops display transfers')

    begin(); watch.send('ECLIPSE_CLOSE')
    closed = current()
    assert closed['page'] == '0' and closed['eclipse'] == '0' and closed['eclipse_intro'] == '0', closed
    checks.append('explicit close cancels the intro and returns to normal home')

    watch.send('TIMER 2'); watch.send('PAGE 3'); time.sleep(.5)
    watch.send('BOOT_LONG')
    assert current()['eclipse_intro'] == '1'
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        timer = current()
        if timer['page'] == '2': break
        time.sleep(.1)
    assert timer['page'] == '2' and timer['sleeping'] == '0' and timer['eclipse_intro'] == '0', timer
    assert timer['eclipse'] == '1'
    checks.append('timer completion interrupts intro and opens its alert without ending Eclipse')
    watch.send('PAGE 0')
    return {'result': 'PASS', 'version': project_version(), 'checks': checks,
            'question': question, 'confirmation': confirmation, 'home': home,
            'manual_confirmation': manual_confirmation, 'manual_home': manual_home,
            'timer_interrupt': timer, 'samples': samples, 'final': current(),
            'read_retries': watch.read_retries,
            'method': 'USB LVGL callbacks and rendered screenshots; physical touch/hit testing pending'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--output', default='build/dev8')
    args = parser.parse_args()
    output = pathlib.Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    watch = Watch(args.port)
    try:
        result = run(watch, output)
        (output / 'eclipse-entry-check.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        print(f"PASS: {len(result['checks'])} Eclipse entry checks; physical touch pending.")
    finally: watch.close()


if __name__ == '__main__': main()
