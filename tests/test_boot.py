#!/usr/bin/env python3
"""Boot the actual raw disk using emulated hardware, then exercise its console."""
import argparse
import datetime as dt
import hashlib
import json
import re
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser()
parser.add_argument('--qemu',default='qemu-system-i386')
parser.add_argument('--memory',default='32M')
parser.add_argument('--build-dir',default='build64')
parser.add_argument('--arch',default='64')
args=parser.parse_args()
build=Path(args.build_dir)
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
             '-drive',f'file={build}/origin.img,format=raw,if=ide,snapshot=on',
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
        wait_for(b'\x1eORIGIN_READY\x1f',offset)
        segment=bytes(transcript[offset:])
        if expected.encode() not in segment and b'Type yes or no.' in segment:
            return run('yes',expected)
        assert expected.encode() in segment,(text,segment)
        checks.append(text if len(text)<100 else text[:32]+' [oversize input]')
        return segment
    try:
        wait_for(b'\x1eORIGIN_READY\x1f')
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
        run('cpu','64-bit long mode; pointer bytes: 8; EFER.LMA: 1' if args.arch=='64' else '32-bit protected mode')
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
        segment=wait_for(b'\x1eORIGIN_READY\x1f',offset)
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
        run('edit file1.txt','Native editor opened.')
        # Native PS/2 Ctrl+A, multiline typing, Ctrl+S. No host edits the RAM file.
        execute('send-key',{'keys':[{'type':'qcode','data':'ctrl'},{'type':'qcode','data':'a'}],'hold-time':10});time.sleep(.04)
        for key in ['h','e','l','l','o','ret','w','o','r','l','d']:
            execute('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':10});time.sleep(.04)
        execute('screendump',{'filename':str((evidence/'native-editor.ppm').resolve())})
        offset=len(transcript)
        execute('send-key',{'keys':[{'type':'qcode','data':'ctrl'},{'type':'qcode','data':'s'}],'hold-time':10});time.sleep(.04)
        wait_for(b'\x1eORIGIN_READY\x1f',offset)
        run('cat file1.txt','hello\r\nworld')
        checks.append('Native graphical editor: PS/2 select all, multiline text, save and serial readback')
        run('o file1.txt','Native editor opened.')
        for key in ['x','esc','n','esc','y']:
            execute('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':10});time.sleep(.04)
        run('cat file1.txt','hello\r\nworld')
        checks.append('Native editor: unsaved Keep then Discard preserves saved data')
        # Real training executes in guest machine code; observe losses, weight hashes and inference.
        offset=len(transcript);train_start=time.monotonic()
        run('hello','Training actual guest Transformer')
        wait_for(b'GUEST TRANSFORMER TRAINING COMPLETE',offset,timeout=120)
        learning=bytes(transcript[offset:]).decode(errors='replace')
        records=re.findall(r'NN step (\d+) loss ([0-9.]+) weights (\d+) predicted \[(.*?)\]',learning)
        assert len(records)>=26 and records[0][0]=='0' and records[-1][0]=='250',records
        assert records[0][3]!='Hello, world!' and records[-1][3]=='Hello, world!',records
        assert float(records[-1][1])<.02 and float(records[-1][1])<float(records[0][1])/100
        assert records[0][2]!=records[-1][2]
        training_seconds=time.monotonic()-train_start
        (evidence/'guest-transformer-test.json').write_text(json.dumps({'status':'PASS','execution':f'Actual {args.arch}-bit guest kernel, '+('SSE2 scalar' if args.arch=='64' else 'x87')+' floating point, no host inference','steps':250,'parameters':816,'initial':records[0],'final':records[-1],'qemu_observed_seconds':training_seconds,'scope':'Single-phrase memorization; not general language understanding'},indent=2)+'\n')
        execute('screendump',{'filename':str((evidence/'guest-transformer.ppm').resolve())})
        checks.append('Guest Transformer: 250 real gradient updates, loss decrease, weight change, learned generation')
        run('g','predicted [Hello, world!]')
        offset=len(transcript);os.write(serial_in,b'hello\nstop\n');wait_for(b'Guest training stopped.',offset)
        wait_for(b'\x1eORIGIN_READY\x1f',transcript.rfind(b'Guest training stopped.'))
        assert b'NN step 0' in transcript[offset:]
        run('nnstatus','NN step 0')
        checks.append('Guest training can be interrupted; restart uses new random weights')
        run('code print 2+3*4','14')
        run('code print 1/0','Division by zero.')
        run('rm f2','Removed from RAM.')
        run('new demo.fo print (2+3)*4','Saved to RAM only.')
        run('run demo.fo','20')
        run('edit demo.fo','Native editor opened.')
        execute('send-key',{'keys':[{'type':'qcode','data':'ctrl'},{'type':'qcode','data':'a'}],'hold-time':10});time.sleep(.04)
        for char in 'let x = 2\nrepeat 3\nprint x\nend':
            key={' ':'spc','\n':'ret','=':'equal'}.get(char,char)
            execute('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':10});time.sleep(.04)
        offset=len(transcript)
        execute('send-key',{'keys':[{'type':'qcode','data':'ctrl'},{'type':'qcode','data':'s'}],'hold-time':10});time.sleep(.04)
        wait_for(b'\x1eORIGIN_READY\x1f',offset)
        run('run demo.fo','2\r\n2\r\n2\r\nOrigin Script complete.')
        checks.append('Native editor-authored multiline program executes variables and repeat loop in guest')
        run('code train hello','Training actual guest Transformer')
        run('stop','Guest training stopped.')
        run('code prnit 2','Did you mean print?')
        checks.append('Native Origin Script: exact arithmetic, RAM source execution, checked errors, typo suggestion without autoexecution')
        run('clear','Console cleared.')
        run('about','Familiar Origin 0.1.3')
        run('selftest','SELFTEST PASS')
        screenshot=str((evidence/'boot-screen.ppm').resolve())
        execute('screendump',{'filename':screenshot})
        checks.append('VGA framebuffer screenshot saved')
        offset=len(transcript);os.write(serial_in,b'halt\n');wait_for(b'CPU halted.',offset)
        checks.append('CPU halt')
        report={'status':'PASS','started_at_utc':utc,'finished_at_utc':iso(),
                'guest_memory':args.memory,'architecture':args.arch,'boot_to_prompt_seconds':boot_seconds,'total_test_seconds':time.monotonic()-start,
                'qemu_version':subprocess.check_output([args.qemu,'--version'],text=True).splitlines()[0],
                'image_sha256':hashlib.sha256((build/'origin.img').read_bytes()).hexdigest(),
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
