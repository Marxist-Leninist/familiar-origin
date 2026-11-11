#!/usr/bin/env python3
from pathlib import Path
import hashlib,sys
BUILD=Path(sys.argv[1] if len(sys.argv)>1 else "build64")
boot = (BUILD/'boot.bin').read_bytes()
kernel = (BUILD/'kernel.bin').read_bytes()
assert len(boot) == 512 and boot[-2:] == b'\x55\xaa', 'invalid BIOS boot sector'
assert 0 < len(kernel) <= 64 * 512, 'kernel exceeds stage-one load window'
image = bytearray(1024 * 1024)
image[:512] = boot
image[512:512+len(kernel)] = kernel
(BUILD/'origin.img').write_bytes(image)
print(f'Image: {len(image)} bytes; boot: {len(boot)}; kernel: {len(kernel)}')
print('SHA256:', hashlib.sha256(image).hexdigest())
