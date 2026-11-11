"""Local-only static server for the Familiar Origin browser VM. Standard library only.

It serves the project's browser-site/dist folder on 127.0.0.1 and nothing else.
Other machines cannot reach it, requests for any other host name are refused,
and the guest VM itself has no network device.

On Windows, start it with open_local.pyw and stop it with stop_local.pyw.
On any system, or in a terminal:  python local/serve.py   (Ctrl+C stops it).
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
import os
import re
import socket
import socketserver
import sys

HOST = '127.0.0.1'
DEFAULT_PORT = 8767


def port_from_environment():
    """The port can be changed for tests; the launcher scripts read the same variable."""
    raw = (os.environ.get('FAMILIAR_ORIGIN_PORT') or str(DEFAULT_PORT)).strip()
    if not (raw.isascii() and raw.isdigit() and 0 < int(raw) < 65536):
        raise SystemExit('FAMILIAR_ORIGIN_PORT must be a port number from 1 to 65535, not %r.' % raw)
    return int(raw)


PORT = port_from_environment()
HERE = Path(__file__).resolve().parent
# This file lives in <project>/local/; the browser VM is <project>/browser-site/dist.
ROOT = (HERE.parent / 'browser-site' / 'dist').resolve()
PID_FILE = HERE / ('server.pid' if PORT == DEFAULT_PORT else 'server-%d.pid' % PORT)

TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.mjs': 'text/javascript; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.txt': 'text/plain; charset=utf-8',
    '.wasm': 'application/wasm',
    '.img': 'application/octet-stream',
    '.bin': 'application/octet-stream',
    '.gz': 'application/gzip',
    '.xz': 'application/x-xz',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
}

# The emulator compiles WebAssembly and starts one helper worker from a blob URL.
CSP = ("default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self' blob:; "
       "style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; "
       "object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'")

SAFE_PATH = re.compile(r'[A-Za-z0-9_-][A-Za-z0-9._-]*(/[A-Za-z0-9_-][A-Za-z0-9._-]*)*')
# The only names this server answers to, with an optional numeric port (stops DNS rebinding).
HOST_OK = re.compile(r'(?:127\.0\.0\.1|localhost)(?::[0-9]{1,5})?', re.IGNORECASE)


class Handler(BaseHTTPRequestHandler):
    server_version = 'FamiliarOriginLocal/1.3.2'
    sys_version = ''
    protocol_version = 'HTTP/1.1'
    timeout = 30  # release idle keep-alive connections

    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        self.respond(False)

    def do_HEAD(self):
        self.respond(True)

    def common_headers(self):
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        # Lets stop_local.pyw stop exactly the process that is answering on this port.
        self.send_header('X-Familiar-Origin-Pid', str(os.getpid()))

    def refuse(self, code, text):
        body = (text + '\n').encode()
        self.send_response(code)
        self.send_header('Content-Type', 'text/plain; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.common_headers()
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(body)

    def wanted_file(self):
        """The file inside ROOT that the request names, or None. A malformed or
        over-long request target is simply 'not found'; it never raises."""
        try:
            parts = urlsplit(self.path)
            if parts.scheme or parts.netloc:
                return None  # browsers send a plain path; a full URL here is not for this server
            rel = unquote(parts.path).lstrip('/')
            if rel == '' or rel.endswith('/'):
                rel += 'index.html'
            # Windows quietly drops a trailing dot from a name, which would give a file a second address.
            if len(rel) > 240 or not SAFE_PATH.fullmatch(rel) or rel.endswith('.') or './' in rel:
                return None
            target = (ROOT / rel).resolve()
            target.relative_to(ROOT)
            return target if target.is_file() else None
        except (ValueError, OSError, RuntimeError):
            return None

    def respond(self, head):
        hosts = self.headers.get_all('Host') or []
        # Only the blank and tab that HTTP allows around a header value are ignored.
        if len(hosts) != 1 or not HOST_OK.fullmatch(hosts[0].strip(' \t')):
            self.refuse(403, 'This server answers only on this computer.')
            return
        target = self.wanted_file()
        try:
            body = target.read_bytes() if target else None
        except OSError:
            body = None
        if body is None:
            self.refuse(404, 'Not found')
            return
        self.send_response(200)
        self.send_header('Content-Type', TYPES.get(target.suffix.lower(), 'application/octet-stream'))
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('Content-Security-Policy', CSP)
        self.common_headers()
        self.end_headers()
        if not head:
            self.wfile.write(body)


class LocalServer(ThreadingHTTPServer):
    # On Windows the usual SO_REUSEADDR lets a second program bind a port that is
    # already in use, so ask for exclusive use instead: a second copy then fails cleanly.
    # Elsewhere SO_REUSEADDR never allows two listeners; it only lets a restarted server
    # take its port back while old connections are still closing, so keep it there.
    allow_reuse_address = os.name != 'nt'
    # The default backlog of 5 makes Windows reset connections when many arrive at once.
    request_queue_size = 128
    noted_errors = 0

    def server_bind(self):
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        # Skip HTTPServer.server_bind: its reverse name lookup can stall for seconds.
        socketserver.TCPServer.server_bind(self)
        self.server_name = HOST
        self.server_port = self.server_address[1]

    def handle_error(self, request, client_address):
        # A browser dropping a connection is routine and stays out of server.log. Anything
        # else gets one short line (no traceback), and only the first 50 per run, so a page
        # that sends junk requests cannot grow the log without limit.
        error = sys.exc_info()[1]
        if isinstance(error, (ConnectionError, TimeoutError)):
            return
        if LocalServer.noted_errors < 50:
            LocalServer.noted_errors += 1
            print('request failed: %s: %s' % (type(error).__name__, str(error)[:200]), flush=True)


def main():
    if not (ROOT / 'index.html').is_file():
        raise SystemExit('Browser VM files not found in ' + str(ROOT))
    try:
        server = LocalServer((HOST, PORT), Handler)
    except OSError as error:
        raise SystemExit('Port %d on this computer is already in use (%s).' % (PORT, error))
    PID_FILE.write_text(str(os.getpid()))
    print('Familiar Origin browser VM is available on this computer at http://%s:%d/' % (HOST, PORT), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        try:
            if PID_FILE.read_text().strip() == str(os.getpid()):
                PID_FILE.unlink()
        except OSError:
            pass


if __name__ == '__main__':
    main()
