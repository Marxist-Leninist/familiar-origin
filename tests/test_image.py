from pathlib import Path
import subprocess
boot=Path('build/boot.bin').read_bytes()
kernel=Path('build/kernel.bin').read_bytes()
image=Path('build/origin.img').read_bytes()
assert len(boot)==512 and boot[-2:]==b'\x55\xaa'
assert 0<len(kernel)<=32768
assert len(image)==1024*1024
assert image[:512]==boot and image[512:512+len(kernel)]==kernel
undefined=subprocess.check_output(['nm','-u','build/kernel.elf'],text=True)
assert not undefined.strip(), undefined
print('IMAGE TEST PASS: signature, size, kernel placement, no unresolved runtime symbols')
