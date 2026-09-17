"""Offline USB tools. Requires pyserial. Does not reset the watch on connection."""
import argparse, datetime, pathlib, struct, time, zlib
import serial
p=argparse.ArgumentParser()
p.add_argument('command', choices=['sync-time','status','page','timer','screen','sleep','wake',
                                  'wifi-setup','wifi-sync','wifi-forget',
                                  'pwr-short','pwr-long','pwr-release','dizzy'])
p.add_argument('value', nargs='?')
p.add_argument('--port',default='COM4')
a=p.parse_args()
s=serial.Serial(port=None,baudrate=115200,timeout=2)
s.dtr=False; s.rts=False; s.port=a.port; s.open()
s.reset_input_buffer()
cmd=a.command.upper()
cmd=cmd.replace('-','_')
if a.command=='sync-time': cmd=f'TIME {int(time.time())} {int(datetime.datetime.now().astimezone().utcoffset().total_seconds())}'
if a.command in ('page','timer'): cmd+=' '+str(int(a.value))
s.write((cmd+'\n').encode()); s.flush()
if a.command=='screen':
    deadline=time.monotonic()+10
    while True:
        line=s.readline()
        if line.startswith(b'AURA_FRAME '): break
        if time.monotonic()>deadline: raise RuntimeError('No screenshot header: '+repr(line))
    _,w,h,stride,size=line.split(); w,h,stride,size=map(int,(w,h,stride,size))
    data=bytearray()
    while len(data)<size:
        part=s.read(size-len(data))
        if not part: raise RuntimeError(f'Incomplete frame {len(data)}/{size}')
        data.extend(part)
    rows=bytearray()
    for y in range(h):
        rows.append(0)
        for x in range(w):
            v=struct.unpack_from('<H',data,y*stride+x*2)[0]
            rows.extend(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
    def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
    out=pathlib.Path(a.value or 'screen.png'); out.write_bytes(png); print(out.resolve())
elif a.command in ('sync-time','status','wifi-setup','wifi-sync','wifi-forget'):
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        line=s.readline().decode(errors='replace').strip()
        if line: print(line)
        if line.startswith('AURA_'):
            if a.command=='sync-time' and line!='AURA_TIME ESP_OK':
                raise RuntimeError('Clock synchronization failed: '+line)
            break
    else: raise RuntimeError('Watch did not respond')
s.close()
