"""Probe a running Familiar Origin local server (local/serve.py) with raw HTTP requests.

Usage: python local/tests/probe_server.py <port>
Prints one line per probe: what was sent, what came back, PASS/FAIL against the expectation.
The probes only read; they are safe to run against a server you are using.
"""
import os
import socket
import sys

PORT = int(sys.argv[1])


def raw(request, host='127.0.0.1'):
    with socket.create_connection((host, PORT), timeout=5) as s:
        s.sendall(request)
        s.settimeout(5)
        data = b''
        try:
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                data += chunk
                if b'\r\n\r\n' in data:
                    head, _, body = data.partition(b'\r\n\r\n')
                    length = None
                    for line in head.split(b'\r\n')[1:]:
                        if line.lower().startswith(b'content-length:'):
                            length = int(line.split(b':', 1)[1])
                    if length is not None and len(body) >= length:
                        break
        except socket.timeout:
            pass
    return data


def status(data):
    try:
        return int(data.split(b'\r\n', 1)[0].split()[1])
    except Exception:
        return None


def get(path, host_header='127.0.0.1:%d' % PORT, method='GET', extra=b''):
    return raw(b'%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n%s\r\n' % (
        method.encode(), path.encode(), host_header.encode(), extra))


results = []


def check(name, data, want, body_must_not=None, body_must=None):
    code = status(data)
    body = data.partition(b'\r\n\r\n')[2]
    ok = code in want if isinstance(want, tuple) else code == want
    if body_must_not is not None and body_must_not in body:
        ok = False
    if body_must is not None and body_must not in body:
        ok = False
    results.append(ok)
    print('%-4s %-52s -> %s (%d body bytes)' % ('PASS' if ok else 'FAIL', name, code, len(body)))


check('GET / with Host 127.0.0.1', get('/'), 200, body_must=b'Familiar Origin | Browser VM')
check('GET / with Host localhost', get('/', 'localhost:%d' % PORT), 200)
check('GET / with Host evil.example (DNS rebinding)', get('/', 'evil.example'), 403, body_must_not=b'<title>')
check('GET / with Host evil.example:%d' % PORT, get('/', 'evil.example:%d' % PORT), 403)
check('GET / with no Host header', raw(b'GET / HTTP/1.0\r\n\r\n'), 403)
check('GET / with Host 127.0.0.1.evil.example', get('/', '127.0.0.1.evil.example'), 403)
check('GET /../serve.py', get('/../serve.py'), 404, body_must_not=b'Local-only static server')
check('GET /..%2f..%2fserve.py', get('/..%2f..%2fserve.py'), 404, body_must_not=b'Local-only static server')
check('GET /%2e%2e/%2e%2e/serve.py', get('/%2e%2e/%2e%2e/serve.py'), 404, body_must_not=b'Local-only static server')
check('GET /..\\..\\serve.py', get('/..\\..\\serve.py'), 404, body_must_not=b'Local-only static server')
check('GET /vendor/..%5c..%5c..%5cserve.py', get('/vendor/..%5c..%5c..%5cserve.py'), 404)
check('GET //C:/Windows/win.ini', get('//C:/Windows/win.ini'), 404, body_must_not=b'[fonts]')
check('GET /C:/Windows/win.ini', get('/C:/Windows/win.ini'), 404, body_must_not=b'[fonts]')
check('GET /C:%5cWindows%5cwin.ini', get('/C:%5cWindows%5cwin.ini'), 404, body_must_not=b'[fonts]')
check('GET /index.html::$DATA (alternate stream)', get('/index.html::$DATA'), 404)
check('GET /assets/origin.img:Zone.Identifier', get('/assets/origin.img:Zone.Identifier'), 404)
check('GET /.git/config (dot-first segment)', get('/.git/config'), 404)
check('GET /vendor (directory, no slash)', get('/vendor'), 404)
check('GET /vendor/ (directory listing)', get('/vendor/'), 404)
check('GET /CON (device name)', get('/CON'), 404)
check('GET /index.html%00.png (NUL)', get('/index.html%00.png'), 404)
check('GET /index.html?x=1 (query ignored)', get('/index.html?x=1'), 200)
check('GET /index.html. (trailing-dot alias)', get('/index.html.'), 404)
check('GET /vendor./libv86.js (dot-ended folder alias)', get('/vendor./libv86.js'), 404)
# Windows file systems ignore case, so this is the same file there; elsewhere it does not exist.
check('GET /INDEX.HTML (case only)', get('/INDEX.HTML'), 200 if os.name == 'nt' else (200, 404))
check('GET / with Host 127.0.0.1:evil.example', get('/', '127.0.0.1:evil.example'), 403)
check('GET / with Host 127.0.0.1:%d@evil.example' % PORT, get('/', '127.0.0.1:%d@evil.example' % PORT), 403)
check('GET / with Host LOCALHOST:%d (case)' % PORT, get('/', 'LOCALHOST:%d' % PORT), 200)
check('GET / with two Host headers', raw(b'GET / HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nHost: evil.example\r\nConnection: close\r\n\r\n' % PORT), 403)
check('GET http://evil.example/ (full URL as target)', get('http://evil.example/'), 404)
for label, value in [('NBSP before localhost', b'\xa0localhost'), ('NBSP after 127.0.0.1', b'127.0.0.1\xa0'),
                     ('NEL after localhost', b'localhost\x85'), ('vertical tab before localhost', b'\x0blocalhost')]:
    check('GET / with Host padded: ' + label, raw(b'GET / HTTP/1.1\r\nHost: ' + value + b'\r\nConnection: close\r\n\r\n'), 403)
check('GET / with Host padded by an ordinary tab', raw(b'GET / HTTP/1.1\r\nHost:\tlocalhost:%d\t\r\nConnection: close\r\n\r\n' % PORT), 200)
check('HEAD /assets/origin.img', get('/assets/origin.img', method='HEAD'), 200)
check('POST / (method not offered)', get('/', method='POST', extra=b'Content-Length: 0\r\n'), (405, 501))
check('PUT /x.html (method not offered)', get('/x.html', method='PUT', extra=b'Content-Length: 0\r\n'), (405, 501))
check('DELETE /index.html (method not offered)', get('/index.html', method='DELETE'), (405, 501))

head = get('/vendor/v86.wasm', method='HEAD').decode('latin-1')
for name, must in [('Content-Type', 'application/wasm'), ('Content-Security-Policy', "script-src 'self' 'wasm-unsafe-eval'"),
                   ('Cross-Origin-Resource-Policy', 'same-origin'), ('X-Content-Type-Options', 'nosniff'),
                   ('Server', 'FamiliarOriginLocal')]:
    line = next((l for l in head.split('\r\n') if l.lower().startswith(name.lower() + ':')), '')
    ok = must in line
    results.append(ok)
    print('%-4s header %-34s %s' % ('PASS' if ok else 'FAIL', name, line[:110]))

# A request under a foreign host name gets a bare 403: nothing in it names this server or its process.
bare = get('/', 'evil.example').decode('latin-1').partition('\r\n\r\n')[0].lower()
for name in ('server:', 'x-familiar-origin-pid:'):
    ok = ('\r\n' + name) not in bare
    results.append(ok)
    print('%-4s refused foreign-host reply has no %s header' % ('PASS' if ok else 'FAIL', name.rstrip(':')))

# The server must not be reachable from other machines: every non-loopback address must refuse.
try:
    others = sorted({info[4][0] for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)} - {'127.0.0.1'})
except OSError as error:
    others = []
    print('SKIP other-address check: this computer\'s own name does not resolve (%s)' % error)
for address in others:
    try:
        with socket.create_connection((address, PORT), timeout=2):
            reachable = True
    except OSError:
        reachable = False
    results.append(not reachable)
    print('%-4s not reachable on this computer\'s address %s' % ('PASS' if not reachable else 'FAIL', address))

print('%d/%d probes passed' % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
