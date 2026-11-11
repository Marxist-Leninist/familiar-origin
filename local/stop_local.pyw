"""Stop the local Familiar Origin server.

Asks the server on port 8767 which process it is, checks with Windows that this
process really holds the port and is Python, and stops exactly that one. Does
nothing when a different program (or nothing at all) is using the port.

    pythonw stop_local.pyw           stop and show a short confirmation
    pythonw stop_local.pyw --quiet   stop without any window

Exit code: 0 stopped (or nothing was running), 1 could not stop it,
2 the port belongs to a different program or the port setting is invalid.
"""
import ctypes
import os
import socket
import struct
import sys
import time
import urllib.error
import urllib.request
from ctypes import wintypes
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_PORT = 8767
RAW_PORT = (os.environ.get('FAMILIAR_ORIGIN_PORT') or str(DEFAULT_PORT)).strip()
PORT = int(RAW_PORT) if RAW_PORT.isascii() and RAW_PORT.isdigit() and 0 < int(RAW_PORT) < 65536 else 0
URL = 'http://127.0.0.1:%d/' % PORT
PID_FILE = HERE / ('server.pid' if PORT in (0, DEFAULT_PORT) else 'server-%d.pid' % PORT)
LOCAL = urllib.request.build_opener(urllib.request.ProxyHandler({}))
QUIET = '--quiet' in sys.argv

PROCESS_TERMINATE = 0x0001
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
SYNCHRONIZE = 0x00100000

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                                ctypes.POINTER(wintypes.DWORD)]
kernel32.TerminateProcess.restype = wintypes.BOOL
kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
kernel32.WaitForSingleObject.restype = wintypes.DWORD
kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
kernel32.CloseHandle.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]


def serving_pid():
    """PID our server reports when it is the program answering on the port, else None."""
    try:
        with LOCAL.open(URL, timeout=2) as reply:
            headers = reply.headers
    except urllib.error.HTTPError as error:
        headers = error.headers
    except Exception:
        return None
    if not (headers.get('Server') or '').startswith('FamiliarOriginLocal'):
        return None
    pid = (headers.get('X-Familiar-Origin-Pid') or '').strip()
    return int(pid) if pid.isdigit() and int(pid) > 4 else None


def listener_pid():
    """PID that Windows says holds the listening socket for PORT on this computer, else None.

    The answer comes from the system, so a program on the port cannot point us at another process.
    """
    try:
        iphlpapi = ctypes.WinDLL('iphlpapi')
        iphlpapi.GetExtendedTcpTable.restype = wintypes.DWORD
        iphlpapi.GetExtendedTcpTable.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD), wintypes.BOOL,
                                                 wintypes.ULONG, ctypes.c_int, wintypes.ULONG]
        size = wintypes.DWORD(0)
        iphlpapi.GetExtendedTcpTable(None, ctypes.byref(size), False, 2, 3, 0)  # AF_INET, listeners with owner PID
        table = ctypes.create_string_buffer(size.value + 4096)
        size = wintypes.DWORD(len(table))
        if iphlpapi.GetExtendedTcpTable(table, ctypes.byref(size), False, 2, 3, 0):
            return None
        for i in range(struct.unpack_from('<I', table)[0]):
            _state, addr, port, _raddr, _rport, pid = struct.unpack_from('<I4sIIII', table, 4 + 24 * i)
            if socket.ntohs(port & 0xFFFF) == PORT and addr in (b'\x7f\x00\x00\x01', b'\x00\x00\x00\x00'):
                return pid
    except (OSError, struct.error):
        pass
    return None


def port_in_use():
    """True when any program accepts connections on the port, even one that never answers."""
    try:
        socket.create_connection(('127.0.0.1', PORT), timeout=0.5).close()
        return True
    except OSError:
        return False


def stop_server_process(pid):
    """Terminate pid when it is a Python process that holds the listening socket.

    Returns 'stopped', 'not-listener' or 'failed'. The process is opened first: while the
    handle is open Windows cannot give this PID to a different process, so the checks and
    the termination are certain to be about the same process.
    """
    handle = kernel32.OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, False, pid)
    if not handle:
        return 'not-listener' if listener_pid() != pid else 'failed'
    try:
        if listener_pid() != pid:
            return 'not-listener'
        size = wintypes.DWORD(1024)
        image = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, image, ctypes.byref(size)):
            return 'failed'
        if not Path(image.value).name.lower().startswith('python'):
            return 'failed'
        if not kernel32.TerminateProcess(handle, 0):
            return 'failed'
        return 'stopped' if kernel32.WaitForSingleObject(handle, 5000) == 0 else 'failed'
    finally:
        kernel32.CloseHandle(handle)


def message(text, icon=0x40):
    if not QUIET:
        ctypes.windll.user32.MessageBoxW(None, text, 'Familiar Origin', icon | 0x10000)


def remove_pid_file():
    try:
        PID_FILE.unlink(missing_ok=True)
    except OSError:
        pass


def main():
    if not PORT:
        message('FAMILIAR_ORIGIN_PORT must be a port number from 1 to 65535, not %r.' % RAW_PORT, 0x30)
        return 2
    pid = serving_pid()
    if pid is None:
        if listener_pid() is not None or port_in_use():
            message('Port %d is being used by a different program, not by the server in this '
                    'folder, so nothing was stopped.' % PORT, 0x30)
            return 2
        remove_pid_file()  # nothing holds the port, so a leftover file names a dead process
        message('The local Familiar Origin server is not running.')
        return 0
    outcome = stop_server_process(pid)
    if outcome == 'not-listener':
        message('Port %d answers as Familiar Origin, but process %d does not hold that port, so '
                'nothing was stopped.' % (PORT, pid), 0x30)
        return 2
    if outcome != 'stopped':
        message('Could not stop the local Familiar Origin server (process %d).' % pid, 0x30)
        return 1
    for _ in range(20):
        if serving_pid() is None:
            break
        time.sleep(0.1)
    else:
        message('Port %d still answers after process %d was stopped.' % (PORT, pid), 0x30)
        return 1
    remove_pid_file()
    message('The local Familiar Origin server has stopped.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
