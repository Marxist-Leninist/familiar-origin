#!/usr/bin/env python3
"""Boot the actual raw disk using emulated hardware, then exercise its console."""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser()
parser.add_argument('--qemu',default='qemu-system-i386')
parser.add_argument('--memory',default='32M')
args=parser.parse_args()
evidence=Path('evidence');evidence.mkdir(exist_ok=True)
start=time.monotonic();utc=dt.datetime.now(dt.timezone.utc).isoformat()
transcript=bytearray();checks=[]
def iso():return dt.datetime.now(dt.timezone.utc).isoformat()
with tempfile.TemporaryDirectory(prefix='origin-vm-') as temp:
    serial_path=str(Path(temp)/'serial')
    os.mkfifo(serial_path+'.in');os.mkfifo(serial_path+'.out')
    serial_in=os.open(serial_path+'.in',os.O_RDWR|os.O_NONBLOCK)
    serial_out=os.open(serial_path+'.out',os.O_RDWR|os.O_NONBLOCK)
    command=[args.qemu,'-accel','tcg','-machine','pc','-m',args.memory,
             '-drive','file=build/origin.img,format=raw,if=ide,snapshot=on',
             '-display','none','-chardev',f'pipe,id=serial,path={serial_path}',
             '-serial','chardev:serial','-monitor','none',
             '-nic','none','-no-reboot','-qmp','stdio']
    proc=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    selector=selectors.DefaultSelector();selector.register(serial_out,selectors.EVENT_READ)
    def wait_for(expected,offset=0,timeout=10):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            if proc.poll() is not None:raise RuntimeError('QEMU exited: '+proc.stderr.read().decode(errors='replace'))
            if expected in transcript[offset:]: return bytes(transcript[offset:])
            for key,_ in selector.select(timeout=0.05):
                chunk=os.read(key.fileobj,65536)
                if chunk:transcript.extend(chunk)
                elif proc.poll() is not None:
                    raise RuntimeError('QEMU exited: '+proc.stderr.read().decode(errors='replace'))
        raise AssertionError(f'Timeout waiting for {expected!r}: {transcript[-2000:]!r}')
    def run(text,expected):
        offset=len(transcript);os.write(serial_in,text.encode()+b'\n')
        wait_for(b'origin> ',offset)
        segment=bytes(transcript[offset:])
        if expected.encode() not in segment and b'Type yes or no.' in segment:
            return run('yes',expected)
        assert expected.encode() in segment,(text,segment)
        checks.append(text if len(text)<100 else text[:32]+' [oversize input]')
        return segment
    try:
        wait_for(b'origin> ')
        assert b'BOOT SELFTEST PASS' in transcript
        first_boot=iso();boot_seconds=time.monotonic()-start
        if not (evidence/'first-boot.json').exists():
            (evidence/'first-boot.json').write_text(json.dumps({'observed_at_utc':first_boot,'qemu_launch_to_prompt_seconds':boot_seconds,'proof':'Actual raw BIOS disk boot reached kernel shell and BOOT SELFTEST PASS.'},indent=2)+'\n')
        checks.append('Raw BIOS boot to kernel prompt')
        run('help','cat write new rm')
        run('h','Shortcuts:')
        run('show files','welcome.txt')
        run('n unique.txt first','Saved to RAM only.')
        run('n unique.txt second','File exists; not overwritten.')
        run('r unique.txt','first')
        run('delete unique.txt','Type yes or no.')
        run('no','Deletion cancelled.')
        run('r unique.txt','first')
        run('d unique.txt','Removed from RAM.')
        run('run selftest','Running built-in utility: selftest')
        run('run edge.exe','External executable not supported.')
        run('about','No network, browser, app ABI or process isolation.')
        run('cpu','32-bit protected mode')
        run('mem','65536 bytes')
        run('alloc 0','Allocation rejected')
        run('alloc -1','Usage: alloc BYTES')
        run('alloc 4294967296','Usage: alloc BYTES')
        run('alloc 1','Allocated 1 bytes.')
        run('alloc 16','Allocated 16 bytes.')
        run('mem','used: 32')
        run('alloc 65505','Allocation rejected')
        run('alloc 65504','Allocated 65504 bytes.')
        run('alloc 1','Allocation rejected')
        run('selftest','SELFTEST PASS')
        run('ls','welcome.txt')
        run('cat welcome.txt','Your own kernel is running.')
        run('write note.txt Hello from the original kernel','Saved to RAM only.')
        run('cat note.txt','Hello from the original kernel')
        run('write note.txt replacement','Saved to RAM only.')
        run('cat note.txt','replacement')
        run('rm note.txt','Removed from RAM.')
        run('cat note.txt','File not found.')
        run('write bad/name reject','Invalid name')
        run('write huge '+('x'*512),'Invalid name or content too long.')
        run('echo '+('x'*700),'Input too long; command discarded.')
        run('echo recovered','recovered')
        run('not_a_command','Unknown command.')
        # File table capacity is 8; boot installed 2 initial files.
        for i in range(6):run(f'write f{i} test','Saved to RAM only.')
        run('write overflow test','RAM filesystem full.')
        run('write f0 updated','Saved to RAM only.')
        run('rm f0','Removed from RAM.')
        run('write recycled test','Saved to RAM only.')
        # Exercise physical PS/2 keyboard path separately from serial input.
        def recv_qmp():
            while True:
                msg=json.loads(proc.stdout.readline())
                if 'event' not in msg:return msg
        recv_qmp()
        def execute(name,arguments=None):
            proc.stdin.write((json.dumps({'execute':name,'arguments':arguments or {}})+'\n').encode());proc.stdin.flush()
            response=recv_qmp();assert 'error' not in response,response;return response
        execute('qmp_capabilities')
        offset=len(transcript)
        for key in ['e','c','h','o','spc','k','e','y','ret']:
            execute('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':10});time.sleep(.04)
        segment=wait_for(b'origin> ',offset)
        assert b'echo key\r\nkey\r\n' in segment,segment
        checks.append('PS/2 keyboard: echo key')
        run('rm f1','Removed from RAM.')
        # Real graphical interaction: move the emulated PS/2 mouse to New File.
        for dx,dy in [(-100,40),(-100,0),(-60,0)]:
            execute('input-send-event',{'events':[{'type':'rel','data':{'axis':'x','value':dx}},{'type':'rel','data':{'axis':'y','value':dy}}]});time.sleep(.05)
        for down in [True,False]:
            execute('input-send-event',{'events':[{'type':'btn','data':{'button':'left','down':down}}]});time.sleep(.05)
        execute('screendump',{'filename':str((evidence/'native-gui-create.ppm').resolve())})
        offset=len(transcript)
        for key in ['ret','g','u','i','ret']:
            execute('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':10});time.sleep(.04)
        wait_for(b'Saved to RAM only.',offset)
        run('cat file1.txt','gui')
        checks.append('Native VGA GUI: mouse New File, keyboard text, create, serial readback')
        run('clear','Console cleared.')
        run('about','Familiar Origin 0.1.1')
        run('selftest','SELFTEST PASS')
        screenshot=str((evidence/'boot-screen.ppm').resolve())
        execute('screendump',{'filename':screenshot})
        checks.append('VGA framebuffer screenshot saved')
        offset=len(transcript);os.write(serial_in,b'halt\n');wait_for(b'CPU halted.',offset)
        checks.append('CPU halt')
        report={'status':'PASS','started_at_utc':utc,'finished_at_utc':iso(),
                'guest_memory':args.memory,'boot_to_prompt_seconds':boot_seconds,'total_test_seconds':time.monotonic()-start,
                'qemu_version':subprocess.check_output([args.qemu,'--version'],text=True).splitlines()[0],
                'image_sha256':hashlib.sha256(Path('build/origin.img').read_bytes()).hexdigest(),
                'checks':checks,'scope':'QEMU software-emulated x86 BIOS VM only; no physical hardware claims.'}
        (evidence/'boot-test.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    finally:
        (evidence/'serial-transcript.txt').write_bytes(transcript)
        proc.terminate()
        try:proc.wait(timeout=5)
        except subprocess.TimeoutExpired:proc.kill();proc.wait()
        os.close(serial_in);os.close(serial_out)
        stderr=proc.stderr.read().decode(errors='replace')
        (evidence/'qemu-stderr.txt').write_text(stderr)
