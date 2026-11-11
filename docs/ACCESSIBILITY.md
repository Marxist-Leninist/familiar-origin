# Human- and agent-friendly design requirements

This document is a design plan unless explicitly marked implemented.

## Implemented since 0.1.0 (still true in 0.1.1)

- The VM boots straight to a single-user console without asking for a password.
- Ordinary typed commands remain visible, with backspace and readable errors.
- The command set is small, discoverable with `help`, and can be driven through serial input.
- Overflow rejects the whole line instead of silently executing a truncated request.
- The console explains that files are temporary and that real application support is absent.

There is no authentication, authorization, sudo, user account or protected secret storage yet. The absence of a password prompt must not be interpreted as completed secure passwordless authentication.

## Future authentication requirements

- Offer a prominent Show/Hide password control, usable by keyboard and assistive technology, not a hidden shortcut.
- Allow an informed user to choose visible entry, including a persistent local preference. Explain shoulder-surfing/screen-sharing exposure when enabling that preference.
- Offer user-controlled sign-in methods where the system can actually implement them safely, including passwordless authenticators and, for explicitly chosen local low-risk setups, automatic sign-in.
- Keep the choice of login authentication separate from disk encryption, administrative authorization and remote access. A convenience choice must not silently disable unrelated protections.
- Make elevation policy understandable and selectable by the owner once a real privilege model exists; do not implement a fake `sudo` that merely hides the lack of isolation.
- Support clear focus, high contrast, scalable text, no surprise timeouts and a recoverable flow if input is interrupted.

## Future agent interface

Add a versioned structured interface with typed requests and results, permission boundaries, cancellation, activity logs and explicit ownership of each request. Human and agent interfaces should operate on the same underlying services. Destructive or externally consequential operations need an enforceable permission model. An external LLM should never be given unrestricted kernel authority by default.

Claude, Codex and Edge require substantial userspace, browser/networking and platform work. A serial command parser alone is not that support.

## Implemented in 0.1.1

Native graphical Files/New File/Memory/Tests/Console actions and a persistent browser command header are available. The browser supports larger console text, standard labeled form controls, filename suggestions and visible command interpretations. Exact aliases and selected natural phrases share guest operations. Only unambiguous read-only spelling corrections execute automatically; destructive changes require explicit confirmation. This is an early prototype, not an accessibility certification. The native bitmap font, 320×200 resolution, relative mouse and lack of screen-reader semantics need substantial work. Browser companion controls provide a more accessible route meanwhile.
