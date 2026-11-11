"""Regression probes for defects found when local/serve.py was attacked on purpose.

Usage: python local/tests/regress_server.py <port> <path-to-server.log>
  a. 40 downloads of the disk image at the same instant must all complete
  b. malformed and over-long request targets must get a clean 4xx reply, not a dropped connection
  c. junk requests and clients that drop mid-download must not grow server.log
"""
import os
import socket
import struct
import sys
import threading
import time

PORT = int(sys.argv[1])
LOG = sys.argv[2]
HOST_LINE = b'Host: 127.0.0.1:%d\r\n' % PORT
results = []


def check(name, ok, detail=''):
    results.append(ok)
    print(('PASS ' if ok else 'FAIL ') + name + ('  | ' + detail if detail else ''))


def request(first_line, timeout=15):
    try:
        with socket.create_connection(('127.0.0.1', PORT), timeout=timeout) as s:
            s.sendall(first_line + b'\r\n' + HOST_LINE + b'Connection: close\r\n\r\n')
            data = b''
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                data += chunk
            return data
    except OSError as error:
        return b'ERR ' + str(error).encode()


def log_size():
    return os.path.getsize(LOG) if os.path.exists(LOG) else 0


# a. burst of parallel downloads
done = []
lock = threading.Lock()


def download():
    data = request(b'GET /assets/origin.img HTTP/1.1')
    good = data.startswith(b'HTTP/1.1 200') and len(data.partition(b'\r\n\r\n')[2]) == 1048576
    with lock:
        done.append(good)


threads = [threading.Thread(target=download) for _ in range(40)]
start = time.time()
for t in threads:
    t.start()
for t in threads:
    t.join()
check('40 simultaneous downloads of the disk image all complete', all(done) and len(done) == 40,
      '%d/40 in %.1f s' % (sum(done), time.time() - start))

# b. malformed targets
before = log_size()
junk = [
    b'GET http://[/ HTTP/1.1',
    b'GET //[zz]/x HTTP/1.1',
    b'GET /' + b'a' * 40000 + b' HTTP/1.1',
    b'GET /%ff%fe%00 HTTP/1.1',
    b'GET /a/' + b'b/' * 3000 + b'c HTTP/1.1',
    b'GET /' + b'A' * 300 + b'.html HTTP/1.1',
]
codes = []
for line in junk:
    for _ in range(10):
        data = request(line)
        codes.append(data[9:12].decode('latin-1') if data.startswith(b'HTTP/') else 'no reply')
check('%d malformed requests each get a clean 4xx reply' % len(codes),
      all(code in ('400', '404', '414') for code in codes), 'replies seen: ' + ', '.join(sorted(set(codes))))

# c. clients that vanish mid-download (connection reset)
for _ in range(50):
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.sendall(b'GET /assets/origin.img HTTP/1.1\r\n' + HOST_LINE + b'\r\n')
    s.recv(1000)
    # linger on, zero seconds: closing sends a reset (the option is two shorts on Windows, two ints elsewhere)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('hh' if os.name == 'nt' else 'ii', 1, 0))
    s.close()
time.sleep(2)
grew = log_size() - before
check('server.log did not grow from junk or dropped requests', grew <= 200, '%d bytes added' % grew)
check('server still answers normally afterwards', request(b'GET / HTTP/1.1').startswith(b'HTTP/1.1 200'))

print('%d/%d regression probes passed' % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
