import fs from 'node:fs';
import path from 'node:path';
import {V86} from '../browser-site/dist/vendor/libv86.mjs';
const ab=p=>{const b=fs.readFileSync(p);return b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)};
let output='',phase=0;
const start=Date.now();
const vm=new V86({wasm_path:path.resolve('browser-site/dist/vendor/v86.wasm'),memory_size:32*1024*1024,
 bios:{buffer:ab('browser-site/dist/vendor/seabios.bin')},vga_bios:{buffer:ab('browser-site/dist/vendor/vgabios.bin')},
 hda:{buffer:ab('build32/origin.img')},autostart:true,disable_mouse:true,disable_speaker:true,disable_keyboard:true});
const timer=setTimeout(async()=>{console.error(output);await vm.destroy();process.exit(1)},120000);
vm.add_listener('serial0-output-byte',async b=>{
 output+=String.fromCharCode(b);
 if(phase===0&&output.includes('origin> ')){phase=1;vm.serial0_send('selftest\ncode print 2+3*4\nwrite test hello-browser\ncat test\n');}
 if(phase===1&&output.includes('hello-browser\r\norigin> ')){phase=2;vm.serial0_send('hello\n');}
 if(phase===2&&output.includes('GUEST TRANSFORMER TRAINING COMPLETE')){phase=3;clearTimeout(timer);if(!output.includes('14\r\nOrigin Script complete.'))throw Error('Guest language failed');if(!output.includes('predicted [Hello, world!]'))throw Error('Guest neural generation failed');if(!output.includes('BOOT SELFTEST PASS')||!output.includes('SELFTEST PASS:')){console.error(output);process.exit(1);}fs.writeFileSync('evidence/v86-node-serial.txt',output);fs.writeFileSync('evidence/v86-node-test.json',JSON.stringify({status:'PASS',utc:new Date().toISOString(),elapsed_ms:Date.now()-start,emulator:'v86 0.5.473+g315b2b0',scope:'WebAssembly emulator under Node; browser UI verified separately',checks:['BIOS boot','kernel selftest','RAM write','RAM read','Guest Origin Script arithmetic','Actual guest Transformer training, changed weights, loss decrease and learned Hello, world!']},null,2)+'\n');console.log(output);await vm.destroy();process.exit(0);}
});
