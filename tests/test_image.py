from pathlib import Path
import subprocess,sys
BUILD=Path(sys.argv[1] if len(sys.argv)>1 else "build64")
boot=(BUILD/'boot.bin').read_bytes()
kernel=(BUILD/'kernel.bin').read_bytes()
image=(BUILD/'origin.img').read_bytes()
assert len(boot)==512 and boot[-2:]==b'\x55\xaa'
assert 0<len(kernel)<=32768
assert len(image)==1024*1024
assert image[:512]==boot and image[512:512+len(kernel)]==kernel
undefined=subprocess.check_output(['nm','-u',str(BUILD/'kernel.elf')],text=True)
assert not undefined.strip(), undefined
print('IMAGE TEST PASS: signature, size, kernel placement, no unresolved runtime symbols')

header=subprocess.check_output(['readelf','-h',str(BUILD/'kernel.elf')],text=True)
assert ('ELF64' if BUILD.name=='build64' else 'ELF32') in header
print('ELF architecture verified:',BUILD.name)
