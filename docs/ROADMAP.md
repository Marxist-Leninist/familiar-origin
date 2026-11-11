# Roadmap with testable exit criteria

Milestone 0 and an initial native graphical interface are implemented. The Transformer now also trains inside the guest, a native plain-text editor works, and the default build is x86-64 with a 32-bit rollback/compatibility target. Later items are engineering goals, not delivery dates or compatibility promises.

0. **Boot an original x86 kernel.** BIOS-to-kernel boot, serial/VGA console, keyboard, bounded memory/file operations and reproducible VM tests. Current milestone, including a native framebuffer GUI and PS/2 mouse.
1. **Make kernel faults diagnosable.** Exception handlers/IDT, interrupt controller, timer, keyboard interrupts, fault reports and tests for deliberate exceptions. Add memory-map discovery before general physical allocation.
2. **Establish real isolation.** Page-level protection, guarded allocations/stacks, user mode, a syscall ABI, scheduler and adversarial boundary tests. Determine a small capability-based service model rather than exposing unrestricted kernel commands.
3. **Add persistent services.** Virtual block driver, crash-safe filesystem, process loader and a minimal userspace. Prove clean and interrupted-write recovery in VMs before any real disk writes.
4. **Support modern PC boot.** UEFI boot (native x86-64 BIOS boot is implemented), framebuffer/input/device discovery, then a named supported PC. Keep the legacy BIOS image as a separate target.
5. **Start ARM64 separately.** AArch64 cross-build, QEMU `virt` boot, UART, MMU, interrupts and the same userspace tests. This is a second architecture port, not an Android-device release.
6. **Choose an actual Android hardware target.** Identify a specific unlockable board/device, its documented boot chain, device tree, storage, display and input. Respect OEM restrictions and obtain explicit approval before unlock/flash/wipe actions. Do not promise all Android devices.
7. **Build application and agent compatibility.** Networking, TLS, graphics, fonts, accessibility, userspace runtimes and a deliberate ABI/compatibility strategy. Prefer documented web/remote clients as an early path; native Edge/Claude/Codex remains a separate compatibility and licensing question.
8. **Prove user readiness.** Real authentication/elevation policy, secure updates, rollback/recovery, accessibility testing, independent security review and a supported-hardware matrix.

A separate Linux-based project is the nearer route to running existing desktop applications. This original-kernel project pursues an independent foundation and claims none of that compatibility or testing.

See [research extensions](FUTURE_RESEARCH.md) for neural task learning, explicit model promotion, SIMD, MCP, dual-stack/DTN networking, decentralized naming and additional hardware targets. These are not supported capabilities today.
