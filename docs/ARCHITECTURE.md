# Architecture at milestone 0.1.1

## Boot path

1. BIOS loads the first 512-byte disk sector at physical address `0x7c00`.
2. The loader normalizes the code segment, establishes a real-mode stack and selects VGA 320×200 indexed-color mode 0x13.
3. It checks INT 13h extensions and reads 64 sectors from LBA 1 to physical `0x10000`. This limits the stored kernel image to 32 KiB; the builder rejects larger images.
4. It enables A20 through port 0x92, installs its own three-entry GDT and enters 32-bit protected mode.
5. Entry code establishes the stack at `0x90000`, clears the kernel BSS and calls the C kernel.
6. The kernel initializes serial/VGA output, its RAM filesystem and self-tests, then serves console commands.

The raw disk is deliberately simple: a boot sector followed immediately by a fixed-size kernel-loading window. It has no partition table, boot manager, filesystem or installer. This is not a bootable ISO.

## Kernel model

Everything runs in ring 0 in one flat address space. The code is linked at `0x10000`; the linker rejects BSS overlap with reserved stack space. ELF read/execute and read/write segments are useful build metadata, but there is no paging or enforced memory protection. BIOS is not called after protected-mode entry.

Polling avoids an interrupt-controller/IDT requirement for this first milestone. It also means the idle console consumes emulated CPU time. There is no exception recovery, scheduler, timer accounting, SMP, userspace, syscall ABI or kernel modules. Unknown CPU exceptions can stop the VM. The intended initial CPU is a CPUID-capable x86 PC, not every historical x86 processor.

## Storage and allocation

The RAM filesystem is a bounded array of eight files. Names are up to 31 ASCII letters, digits, dots, underscores or hyphens. Contents are at most 511 bytes and names/data are copied only after validation. Updates preserve the prior file on an invalid write. No operation writes to the boot disk.

The kernel arena reserves 65,536 bytes and uses monotonic 16-byte-aligned allocation. Allocation size and arithmetic are checked. `alloc` demonstrates actual allocation and touches its first and last bytes. There is no free; reboot resets it. This allocator does not discover or manage physical system memory.

## Console and automation

The same shell accepts printable ASCII from COM1 or PS/2. COM1 uses 115200 baud, 8-N-1. The native GUI writes to framebuffer 0xa0000 using an original 5×7 font and a 64,000-byte shadow buffer. Its display font renders lowercase as uppercase, but file data preserves case. The line buffer has 639 usable characters. An oversized line is discarded in full when submitted rather than executing a truncated command. Serial CRLF is treated as one newline. Backspace edits an ordinary line; after overflow, submit it and enter a fresh command.

The guest initializes a standard PS/2 relative mouse and polls its packets. Graphical buttons and shell aliases call the same command dispatcher. Native file creation asks for a name and up to 120 printable characters; the underlying filesystem and browser form support 511 bytes. The browser companion uses a control-delimited ORIGIN_READY marker to serialize requests. Names/payloads cannot contain the marker bytes. This is a prototype bridge, not a secure RPC boundary. Finish guest modal dialogs before using the companion controls.

The console is scriptable, as demonstrated by the tests. It is not yet a stable machine-readable API or an AI agent service. There is no network device in the supplied run/test command.

## Original versus reused

Project code includes the BIOS loader, protected-mode entry, C kernel/core, shell, tests and build scripts. No Linux, BSD, Android or other existing kernel code is linked. Existing GCC/binutils, the Python interpreter, QEMU and SeaBIOS are development/firmware infrastructure. They are not claimed as inventions of this project.

Deletion is staged: rm/remove/delete/d names the RAM file, then requires yes/no. Native graphical deletion has a target-specific confirmation dialog. The new command rejects an existing filename; ordinary write can replace one. The browser additionally confirms overwrites. All data disappears on reboot.
