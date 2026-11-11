# Measurement ledger

All timestamps below are recorded UTC observations. The task’s initial filesystem timestamp and the execution environment’s display timezone can differ; the evidence uses explicit UTC clock readings.

- Timer recorded: `2026-11-11T06:29:48Z`.
- First actual boot-to-shell observation: `2026-11-11T06:36:54.944046+00:00`.
- Elapsed to first working kernel shell: **426.944 seconds (7 minutes 6.944 seconds)**.
- First boot disk SHA-256: `849b50e9412f5bdf7e942fcc39649ea3f84b4772f73d8f270e7cede49ab563ac`.
- First kernel payload: 5,024 bytes, plus a 512-byte original boot sector.
- First VM suite: 35 recorded checks, including PS/2 key injection, RAM-file operations, overflow rejection, screenshot and halt.
- Subsequent version: `alloc` command and separate ELF segments, 8,204-byte kernel; 44 VM checks. See current `evidence/boot-test.json` (the 32-bit run; `evidence/boot-test-64.json` is the 64-bit one) for image identity and final validation time.

The first-boot image hash, the 5,024-byte and 8,204-byte kernel sizes and the 35- and 44-check suites were noted when those earlier builds ran. Those builds are not in this bundle, so the figures cannot be reproduced from it; `evidence/first-boot.json` holds the first-boot timestamp only.

The timer starts when this implementation workspace was created. It includes implementation, dependency preparation and troubleshooting. It is wall-clock elapsed time, not developer CPU time. Later documentation, publication, packaging and review are not included in the first-boot number.

## Reproducibility and limitations

The original environment already had GCC/binutils, Make and Python but no QEMU. Official Debian packages were downloaded, SHA-256 checked against the Debian package index and extracted into a project-local tools directory. No system package installation, device change, cloud rental, paid API call or physical-disk write was required. `evidence/qemu-packages.json` records package provenance. Those QEMU binaries are not bundled. The later browser package bundles v86 and SeaBIOS under their own licenses with notices/source, as documented in its licenses page.

Initial infrastructure failures were an unsupported local Unix socket, an IDE read-only option and a missing firmware search path. The final test uses QMP over subprocess pipes, serial over local FIFOs, a temporary disk snapshot and an explicit firmware path in the local wrapper. These were test-environment setup failures, not evidence that the image had booted before the first successful record.

Host tests use AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer cannot operate under this container’s tracing environment, so leak detection is disabled and disclosed. The kernel itself has no sanitizer runtime. It is checked separately by the emulator tests and build/image assertions.

The final disk was rebuilt and checked for byte-for-byte reproducibility with the same toolchain. This does not establish reproducibility across different compiler versions.

## What the comparison does not establish

There is no matched-scope measurement against Microsoft, Apple, Google or another operating-system team. A toy/research kernel can be made to boot quickly precisely because it omits most product requirements. A seven-minute minimal boot milestone does not measure time to a safe general-purpose OS, engineering effort, commercial feature parity, supported hardware breadth, reliability or security.

## Later browser milestone

A real browser VM was added using existing v86 WebAssembly emulation, not a new emulator. The same disk also booted under v86 in Node. Chromium UI testing of the first browser build confirmed boot, built-in selftest, RAM file create/read, typed input, pause/resume, restart cancellation, confirmed restart clearing RAM, and the VGA display. That run was noted at the time and is not recorded in this bundle. The initial hosted deployment succeeded at 2026-11-11 06:52:34 UTC, about 22 minutes 46 seconds after the implementation timer. This later milestone is separate from the seven-minute first QEMU boot. For 0.1.1 the same list was repeated by `tests/test_browser.mjs` in Microsoft Edge on Windows 11 against a local server serving the same files, and in headless Chromium on Linux against the public page; those two reports are kept in the `v0.1.1` tag. For 0.1.3 the script, which now also covers guest training and one Origin Script statement, was run again the same two ways, and `evidence/browser-qa.json` and `evidence/browser-qa-public.json` record the 0.1.3 runs. Physical-device testing remains unverified.

The public deployment is https://zqx.lat/familiar-origin/.

## 0.1.1 extension

Native VGA graphics and PS/2 mouse file creation passed the integrated QEMU test at 2026-11-11 07:17:08 UTC. This is a later milestone, not included in the 7-minute first-boot measurement. The separate 816-parameter Transformer passed three seeds and 18 finite-difference gradient checks in Node; see evidence/transformer-test.json. Browser training also visibly reached 250 steps, loss 0.000077 and predicted Hello, world! in Chromium, reporting 0.57 seconds including intentional yielding; those three figures were noted at the time. The 0.1.1 run in Edge (its report is in the `v0.1.1` tag) checked the 250 steps and the predicted text only. That is one-phrase memorization, not an OS-performance or general-intelligence benchmark.

## 0.1.2 native execution

For 0.1.2 both the x86-64 and x86-32 QEMU suites passed 69 checks. The 64-bit guest reports eight-byte pointers and EFER.LMA=1. A 250-step 816-parameter training run took approximately 0.276 seconds under QEMU x86-64 and 2.139 seconds under QEMU i386, including progress output. Those 0.1.2 figures were noted at the time; the 0.1.2 suite and timing reports are not in this bundle. The per-architecture guest-transformer-test reports that are in this bundle are the 0.1.3 runs and record about 0.375 and 2.082 seconds. These different emulation/math paths are not a hardware or Windows/Linux performance comparison. The browser compatibility guest also trained successfully in v86, with no host neural inference; `evidence/browser-qa-0.1.2.json` is the 0.1.2 record of that check in a cloud Chromium.

Pointer tests execute 1,000 movements through the actual renderer and compare the resulting framebuffer byte-for-byte. Mouse-only updates write at most 60 framebuffer bytes, compared with 64,020 before. This measures the code path, not end-to-end input latency, polling delay, display refresh or Windows cursor behavior.

## 0.1.3 language milestone

Both QEMU architecture suites now pass 81 checks, including an editor-authored multiline Origin Script program, variable/loop execution and the language action that starts guest training. The host interpreter tests cover checked arithmetic, limits and errors. No interpreter throughput comparison is claimed.

The two release images were also run on a second machine under an older QEMU (8.2.2), and both builds were rebuilt there from the same source with GCC 13.3.0. All four images pass the same 81 checks, and the host and Node tests pass there as well; `evidence/second-machine-check.json` records that run. The rebuilt images differ byte for byte from the release images because the compiler version differs. The timings in that file are observations under emulation, not benchmarks.
