# Architecture at 0.1.3

## Native x86-64 default

BIOS loads the original 512-byte boot sector at 0x7c00. It selects mode 0x13, reads a fixed 32 KiB kernel window at 0x10000, enables A20 and enters protected mode. The 64-bit entry checks CPUID long-mode support, builds identity page tables at 0x1000/0x2000/0x3000 covering the first GiB with 2 MiB pages, enables PAE and EFER.LME, then enables paging and far-jumps to a 64-bit code segment. It initializes x87/SSE state, clears BSS and calls the 64-bit C kernel with a 16-byte-aligned stack.

The CPU command reports pointer size and reads EFER.LMA, not merely the CPU's advertised capability. ELF class and target are independently checked in the build. This remains BIOS boot, not UEFI. The identity mapping is writable/executable ring-0 memory and provides no process protection.

## 32-bit compatibility

The same core sources also compile to a separately tested protected-mode kernel. That target uses x87 scalar floating point. v86 cannot execute x86-64 long mode, so only this compatibility image is loaded by the browser. Both images have their own hashes and QEMU reports.

## Device and GUI model

PS/2 keyboard/mouse and COM1 are polled. Interrupts remain disabled. A 320×200 indexed-color framebuffer at 0xa0000 uses an original 5×7 upper/lowercase font and a 64,000-byte shadow buffer. UI actions and shell commands use the same dispatcher and RAM filesystem. Mouse-only movement restores the previous cursor from the shadow buffer and draws the new cursor, without repainting the desktop. No Windows input-latency equivalence has been established.

The native editor keeps a bounded 511-byte draft and a copy of the original. It supports multiline text, basic cursor movement and select-all. Save rejects an unexpectedly changed/deleted source file and never silently truncates. Escape/Close asks before discarding unsaved changes. RAM files have no disk persistence.

The browser companion uses control-delimited ORIGIN_READY records to serialize shell requests. Asynchronous training progress never emits that completion marker. Finish/cancel a native editor or modal before using companion serial controls; this bridge is not a secure RPC API.

## Neural computation

The guest C model has 816 float32 weights, gradients and Adam state. All arrays are statically bounded. It uses learned token/position embeddings, one causal attention head, a residual/feedforward block and output projection. Forward, backpropagation, updates and greedy generation run in the kernel. The training target is one known phrase; inference generates from the weights, not a fixed output string.

A training command initializes a fresh seed and starts at most 250 steps. The main loop polls input between steps. Progress reports appear every ten steps and include loss, generated output and a non-cryptographic weight fingerprint. Stop preserves current RAM weights; restarting clears them. A separate browser/Node implementation is reference code only.

Current neural computation is not an action parser, agent, continual trainer or security decision-maker. Existing command suggestions remain deterministic. Future promotion criteria are described in FUTURE_RESEARCH.md.

## Bounds and absence of isolation

Eight RAM files hold names up to 31 ASCII letters/digits/dot/underscore/hyphen and contents up to 511 bytes. New rejects an existing name. Delete stages the named target and requires confirmation. A new graphical action cancels a stale pending deletion before choosing another target.

The arena reserves 64 KiB with checked 16-byte-aligned monotonic allocation; it is not a general physical-memory manager. The linker rejects BSS overlap with the reserved stack area. The loader rejects kernels larger than 32 KiB. There is no user mode, scheduler, fault handler, filesystem driver, networking or supported third-party executable ABI. Unknown faults may stop the VM. This is unsuitable for sensitive data or operational spacecraft.

## Origin Script v0

The native interpreter executes .fo files from the RAM filesystem or one-line code statements. It has checked signed-32-bit arithmetic, eight variables, bounded loops, quoted print strings and guest-training action callbacks. Source and execution limits are enforced independently of the GUI. Tests type a multiline program through actual PS/2 input in the native editor, save it, and execute its variables and loop in each guest architecture. It is not Python, does not load third-party libraries and does not use a neural parser.
