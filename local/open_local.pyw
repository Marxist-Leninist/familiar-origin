"""Open the local Familiar Origin browser VM.

Starts the local-only server when it is not already running, waits until it
answers, then opens http://127.0.0.1:8767/ in Microsoft Edge (or the default
browser when Edge is missing). Run with pythonw so no console window appears.

    pythonw open_local.pyw               start if needed, then open the page
    pythonw open_local.pyw --no-browser  start if needed, open nothing

Exit code: 0 the page is being served, 1 it could not be started,
2 the port setting is invalid.

The server keeps running after this script ends. A caller must wait for this
script only, never for its whole process tree: PowerShell's "Start-Process -Wait"
does not return until the server stops. Use -PassThru and $p.WaitForExit().
"""
import ctypes
import msvcrt
import os
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
import webbrowser
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_PORT = 8767
RAW_PORT = (os.environ.get('FAMILIAR_ORIGIN_PORT') or str(DEFAULT_PORT)).strip()
PORT = int(RAW_PORT) if RAW_PORT.isascii() and RAW_PORT.isdigit() and 0 < int(RAW_PORT) < 65536 else 0
URL = 'http://127.0.0.1:%d/' % PORT
LOG = HERE / 'server.log'
EDGE_PATHS = [
    r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
    r'C:\Program Files\Microsoft\Edge\Application\msedge.exe',
]
# Never send the check for our own local server through a system proxy.
LOCAL = urllib.request.build_opener(urllib.request.ProxyHandler({}))
QUIET = '--quiet' in sys.argv


def server_state():
    """What is answering on the port.

    'ours'  this folder's server
    'page'  another local server that is already showing the Familiar Origin page
            (for example one started by a different tool from its own copy)
    'other' a different program
    'down'  nothing
    """
    try:
        # Windows takes 2 seconds to report a refused connection; a listening port accepts at once.
        socket.create_connection(('127.0.0.1', PORT), timeout=0.5).close()
    except OSError:
        return 'down'
    for patience in (2, 6):  # a second, longer try: our own server can be slow on a busy computer
        try:
            with LOCAL.open(URL, timeout=patience) as reply:
                if (reply.headers.get('Server') or '').startswith('FamiliarOriginLocal'):
                    return 'ours'
                start = reply.read(4096).decode('utf-8', 'replace')
                return 'page' if '<title>Familiar Origin | Browser VM</title>' in start else 'other'
        except urllib.error.HTTPError as error:
            return 'ours' if (error.headers.get('Server') or '').startswith('FamiliarOriginLocal') else 'other'
        except Exception:
            pass
    return 'other'  # something holds the port but does not answer as a web server


def message(text):
    if QUIET:
        return
    ctypes.windll.user32.MessageBoxW(None, text, 'Familiar Origin', 0x30 | 0x10000)


def open_log():
    """server.log for the server's output, as a file descriptor.

    Append-only access makes Windows put every write at the end of the file. A plain 'ab'
    handle inherited by a child keeps writing at the offset the file had when the launcher
    opened it, so two servers starting together overwrite each other's lines.
    """
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreateFileW.restype = ctypes.c_void_p
    kernel32.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    # FILE_APPEND_DATA, share read+write, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL
    handle = kernel32.CreateFileW(str(LOG), 0x0004, 0x0003, None, 4, 0x80, None)
    if handle is None or handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    return msvcrt.open_osfhandle(handle, os.O_APPEND)


def start_server():
    exe = Path(sys.executable)
    pythonw = exe.with_name('pythonw.exe')
    flags = 0x00000008 | 0x00000200  # detached from any console, own process group
    try:
        log = open_log()
    except OSError:
        log = None  # no log is better than no server
    try:
        return subprocess.Popen(
            [str(pythonw if pythonw.exists() else exe), str(HERE / 'serve.py')],
            cwd=str(HERE), stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL if log is None else log,
            stderr=subprocess.DEVNULL if log is None else log,
            creationflags=flags, close_fds=True,
        )
    finally:
        if log is not None:
            os.close(log)


def log_since(offset):
    """The last line the server wrote to server.log after the given offset: its reason for stopping."""
    try:
        with open(LOG, 'rb') as log:
            log.seek(offset)
            lines = log.read(4000).decode('utf-8', 'replace').strip().splitlines()
        return lines[-1][:300] if lines else ''
    except OSError:
        return ''


def main():
    if not PORT:
        message('FAMILIAR_ORIGIN_PORT must be a port number from 1 to 65535, not %r.' % RAW_PORT)
        return 2
    state = server_state()
    if state == 'other':
        message('Port %d on this computer is being used by another program, so the local '
                'Familiar Origin page cannot start. Close that program and try again.' % PORT)
        return 1
    if state == 'down':
        try:
            offset = LOG.stat().st_size
        except OSError:
            offset = 0
        server = start_server()
        deadline = time.monotonic() + 20  # a busy computer can be slow to start Python
        while time.monotonic() < deadline:
            time.sleep(0.25)
            exited = server.poll() is not None  # serve.py gave up (files missing, port taken)
            state = server_state()
            if state in ('ours', 'page') or (exited and state == 'down'):
                break
        if state not in ('ours', 'page'):
            reason = log_since(offset)
            message('The local Familiar Origin server did not start.\n\n'
                    + (reason + '\n\n' if reason else '') + 'Log: ' + str(LOG))
            return 1
    if '--no-browser' in sys.argv:
        return 0
    for edge in EDGE_PATHS:
        if os.path.exists(edge):
            subprocess.Popen([edge, URL], close_fds=True)
            return 0
    webbrowser.open(URL)
    return 0


if __name__ == '__main__':
    sys.exit(main())
