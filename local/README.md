# Run the browser VM on your own computer

The browser VM in `browser-site/dist` has to be served over HTTP; opening `index.html` as a file does not load WebAssembly and the disk image reliably. The scripts in this folder serve it to this computer only and open it for you. They use only the Python standard library (written for Python 3.9 or newer, tested with 3.12), install nothing and change no system settings.

## Start and stop

**Windows.** Double-click `open_local.pyw`, or run:

```
pythonw local\open_local.pyw
```

It starts the server if it is not already running, waits until it answers and opens http://127.0.0.1:8767/ in Microsoft Edge (or your default browser). Press **Start VM** on the page. The server keeps running until you sign out, restart, or run:

```
pythonw local\stop_local.pyw
```

**Any system, in a terminal.**

```
python3 local/serve.py
```

Then open http://127.0.0.1:8767/ yourself. Ctrl+C stops it.

| Script | Exit code |
| --- | --- |
| `open_local.pyw` | 0 the page is being served, 1 it could not be started, 2 the port setting is invalid |
| `stop_local.pyw` | 0 stopped or nothing was running, 1 could not stop it, 2 the port belongs to a different program or the port setting is invalid |

Add `--quiet` to suppress message boxes and `--no-browser` to start the server without opening a page. Set the environment variable `FAMILIAR_ORIGIN_PORT` (1 to 65535) to use another port; all three scripts read it.

`open_local.pyw` leaves the server running after it ends. A caller must wait for that script only, not for its process tree: in Windows PowerShell, `Start-Process -Wait` does not return until the server stops. Use `-PassThru` and `$p.WaitForExit()`.

## What the server does and does not do

- Listens on 127.0.0.1 only. Other computers cannot connect.
- Answers only requests whose `Host` header is `127.0.0.1` or `localhost` with an optional numeric port, and only when there is exactly one such header. This stops a web page on another site from reaching it by pointing a host name at 127.0.0.1.
- Serves files inside `browser-site/dist` and nothing else. Request paths may contain only letters, digits, `.`, `_` and `-`, a path segment may not start with a dot or end with one, and the file must still be inside that folder after links are resolved. Everything else is `404`.
- Supports `GET` and `HEAD` only. No directory listings, no uploads.
- Sends a Content-Security-Policy that allows the page's own scripts, WebAssembly and one worker, and nothing from other origins.
- Takes the port for itself, so a second copy exits with a clear message instead of sharing the port.
- Does not log requests. `server.log` holds start-up lines and, at most, the first 50 unexpected errors of a run.
- `stop_local.pyw` stops a process only when the server on the port reports that process id, Windows confirms that this process holds the listening socket, and the process is Python.

It is a small convenience for a research prototype, not a production web server: no TLS, no authentication, no caching, one process.

## Tests

With the server running on port 8767:

```
python3 local/tests/probe_server.py 8767
python3 local/tests/regress_server.py 8767 local/server.log
```

`probe_server.py` sends about 40 raw requests (other host names, path tricks, Windows file-name aliases, unsupported methods, padded `Host` values), checks the response headers, and checks that the server is not reachable on the computer's other addresses: 47 checks on the Windows test machine, a few more or fewer elsewhere depending on how many addresses the computer has. `regress_server.py` repeats four faults that an earlier version had: connection resets under 40 simultaneous downloads, empty replies to malformed targets, log growth from junk requests, and survival afterwards.

These scripts were written and attacked on 2026-11-11 on Windows 11 with Python 3.12 and Microsoft Edge 158. Two independent test passes on private copies found the defects that the regression script now guards against; the final server version was then re-attacked and no way was found to read a file outside `browser-site/dist`, reach it from another machine, or run a second copy on the same port. That is evidence for these narrow properties on that one machine, not a security audit.
