"""Capture release screens after radios are idle; excludes network scans/SSID."""
import argparse
import json
import pathlib
import time
from watch import Watch
from features_check import wait_status


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--port',default='COM4')
    parser.add_argument('--output',default='build');args=parser.parse_args()
    output=pathlib.Path(args.output);output.mkdir(parents=True,exist_ok=True);watch=Watch(args.port)
    try:
        wait_status(watch,lambda s:s['radio']=='0',60);time.sleep(1.1)
        watch.send('WAKE');watch.send('ECLIPSE_CLOSE');watch.send('PAGE 3')
        def shot(name):
            time.sleep(0.6);watch.screenshot(output/name)
        watch.send('MENU_SCROLL -2000');shot('menu-top.png')
        watch.send('MENU_SCROLL 2000');shot('menu-bottom.png')
        watch.send('PAGE 13');shot('battery.png')
        watch.send('PAGE 3');watch.send('BOOT_LONG');watch.wait_eclipse_ready();watch.send('PAGE 6');shot('eclipse.png')
        watch.send('MENU_SCROLL 2000');shot('eclipse-bottom.png')
        watch.send('PAGE 12');watch.send('CIDR 192.168.1.10/24');shot('cidr.png')
        watch.send('PAGE 15');watch.send('RF 2400,100,-20');shot('rf.png')
        watch.send('ENGINEER_KEY -1');shot('engineer-keyboard.png')
        watch.send('ENGINEER_KEY 17');time.sleep(0.2)
        watch.send('PAGE 16');watch.send('VLSM 192.168.1.0/24,50,20,10');shot('vlsm.png')
        watch.send('PAGE 9');shot('evidence.png')
        watch.send('PAGE 0');shot('home-hacker.png')
        result={'result':'PASS','version':watch.display()['version'],'radio':'0',
                'screens':11,'read_retries':watch.read_retries,'final':watch.display(),
                'scope':'LVGL screenshots from actual MCU, excluding scan data and configured SSID'}
        (output/'release-previews.json').write_text(json.dumps(result,indent=2)+'\n')
        print('11 release previews captured with radios idle')
    finally:watch.close()


if __name__=='__main__':main()
