# Research targets, not current capabilities

## Shared human and agent actions

The native GUI and shell already call the same kernel command dispatcher and RAM filesystem. The browser companion translates a bounded set of phrases into those commands. Single-letter aliases, intermediate phrases and full phrases should cover every future action. Current coverage is partial and explicitly tested. Unknown inputs produce suggestions; only unambiguous read-only typos are auto-corrected, visibly. Names and file contents are never rewritten by spelling correction. Delete/remove/rm all require a named-target confirmation.

A future action registry should carry a stable action ID, typed arguments, side effects, undo policy, confirmation policy and GUI presentation. Parsing proposes an action; a deterministic validator and permission layer decides whether it can execute. A statistical character/word model is a reasonable small baseline against edit distance and a Transformer. Evaluate on independently authored synthetic misspellings, transpositions, missing words, ambiguous phrases and adversarial inputs. Do not publish private command histories as datasets.

## Transformer-first assistance and continual training

The shipped browser lab trains an actual 816-parameter causal Transformer from random weights to memorize one phrase. It is separate from the guest kernel. It has no OS task-training data, persistent background training, model promotion or command authority.

Next experiment: generate synthetic natural-language/action pairs from the deterministic registry, hold out entire phrase templates and typo families, then train a small proposal model. Compare it with deterministic and statistical baselines for exact action/argument accuracy, calibration, abstention, latency and memory. Training loss alone is not a promotion criterion. A model must pass held-out intent tests, payload preservation, unknown-command rejection, destructive-action confirmation and all existing regressions before even shadow deployment. Require explicit choice before replacing suggestions. Keep a frozen known-good model and immediate rollback.

Continual mode should be opt-in, with inspectable local-only examples, exclusion of secrets and sensitive documents, deletion/export controls, a CPU/memory/energy budget, idle scheduling and immediate yield to input. Train a candidate checkpoint without changing the active model. Evaluate and stage each promotion. Always-on availability does not mean unrestricted full-speed training or silently collecting everything the user types. The current demo never collects history and stops when hidden.

## Native performance and language experiments

Current guest code is compiled native 32-bit x86 plus boot/input assembly. SIMD is disabled; CPUID display is not an AVX implementation. Before AVX use, verify CPUID XSAVE/OSXSAVE/AVX, configure OS state handling and check XGETBV XCR0 bits. AVX2/AVX-512 need their extra feature/state checks, context preservation and fallback tests. ARM NEON/SVE and RISC-V vector targets need their own detection and ABI handling. Do not run unknown instructions speculatively.

Benchmark scalar, compiler-vectorized and selected explicit-vector kernels for correctness and end-to-end useful work, not just peak arithmetic. A possible new language should begin as an inspectable typed action/dataflow DSL, compile into existing safe interfaces, and earn its place against C/Rust/Python baselines. No new language or fastest-performance result is implemented.

## Network and assistant-control design

No guest network stack, IPv4, IPv6, TCP, TLS or MCP server exists. Supplied VMs have no guest network relay. First build a bounded virtual NIC driver and packet parser with fuzz tests, then dual-stack addressing and explicit user-enabled services. Network-facing control requires real memory/process isolation, authentication, capability scopes, request limits and revocation.

A local-only MCP bridge could expose read-only status first, then separately scoped create/read/remove actions. Bind loopback by default, protect against browser-origin/CSRF and DNS-rebinding attacks, show the exact pending mutation, and log actions without secrets. No listener, credential, firewall rule or remote persistent access is created by this prototype. Such access needs its own explicit setup and approval.

## Naming beyond a single DNS authority

Self-certifying public-key identifiers and content-addressed records are research options, not a claim to eliminate trust, key recovery, ownership disputes or abuse. Human-readable names still need a collision and governance policy. Start with a closed simulation, interoperability and signed-record verification, not an Internet-wide deployment.

For space systems, use delay/disruption-tolerant store-and-forward networking (for example Bundle Protocol version 7 concepts), contact plans, expiry, custody/retry policy, bounded queues and offline identity verification. Light-speed delay remains unavoidable: a more ambitious address space cannot create real-time interplanetary communication. Universe-scale routing is speculative; there is no established communications mechanism for a multiverse.

## Hardware and software targets

Only an emulated x86 BIOS PC is supported. Modern x86 UEFI, ARM64, Android boards, Apple Silicon, AMD/Intel graphics, NVIDIA CUDA, Python and real spacecraft/satellite targets are not supported. Each requires an explicit architecture/board port, boot-chain documentation, licensing review and drivers or an existing supported runtime. A GPU vendor name is not a portable driver specification. Spacecraft require independent safety, reliability, radiation and real-time qualification; this research kernel is unsuitable for operational use.

Prefer reversible user-space VM packages on an existing OS before installation. Never write bootloaders, repartition, flash firmware, unlock a phone or change host security settings as a side effect of development. A future installer needs tested backups, rollback and an explicit chosen device. Testing an emulator on a laptop is not a laptop hardware certification.

## Security choice

Aim for safe defaults with clearly explained owner choices and recovery routes. A configurable system must still accurately describe what protection is absent. This prototype has no actual authentication, isolation or privilege model to toggle. No user preference about guest policy changes the permissions of the host or of third-party services.

## Memory tiers and paging research

A future normal mode should combine ordinary CPU caching, RAM and persistent storage rather than pretending they are interchangeable. RAM-only operation is plausible after boot, with clear volatility and recovery choices. Storage-backed low-RAM operation still needs working memory for the kernel, page tables, DMA and I/O buffers; an SSD is not a direct substitute for physical RAM. Cache-as-RAM is specialized firmware/hardware behavior, not an assumed general-purpose CPU mode.

The current kernel has a fixed RAM arena, no paging, swap, persistent block driver or cache-management policy. Future benchmarks must use matched hardware and workloads, include tail input latency, task completion, throughput, memory pressure, energy, bytes written and write amplification, and compare well-configured Windows/Linux baselines. SSD wear depends on the actual device and workload. No paging superiority has been demonstrated.

## Fault injection and recovery

There is currently no exception handler, crash recovery or radiation tolerance. A future isolated test harness should inject reproducible register, memory and instruction-stream bit faults in disposable VM snapshots, then report crashes, silent corruption, detected/recovered failures and data loss against an unprotected baseline. Add checksummed state, bounded validation, journal/checkpoint recovery and watchdogs one at a time. AI diagnosis should summarize verified logs, never invent a repair or overwrite evidence.

Representative terrestrial and space fault rates require published sources and actual device/orbit/environment assumptions; arbitrary flips are stress tests, not radiation qualification. Software does not replace radiation-hardened components, ECC, shielding, redundancy or mission-specific certification. No actual user hardware is deliberately damaged, corrupted or exposed to radiation.
