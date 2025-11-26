#!/usr/bin/env python3
"""
Stress test script for the echo server.

Usage: edit parameters below or run as-is.

This script opens `NUM_CLIENTS` concurrent TCP clients to localhost:8080.
Each client sends `MSG_PER_CLIENT` messages of size `MSG_SIZE` and measures
per-message round-trip latency. At the end it prints throughput and
latency percentiles (p50/p95/p99).
"""
import socket, threading, time, argparse
import statistics
from collections import deque

parser = argparse.ArgumentParser(description='Stress test for echo server')
parser.add_argument('--host', default='127.0.0.1')
parser.add_argument('--port', type=int, default=8080)
parser.add_argument('--clients', type=int, default=200)
parser.add_argument('--msgs', type=int, default=200)
parser.add_argument('--size', type=int, default=256)
parser.add_argument('--connect-timeout', type=float, default=2.0)
parser.add_argument('--io-timeout', type=float, default=5.0)
args = parser.parse_args()

HOST = args.host
PORT = args.port
NUM_CLIENTS = args.clients
MSG_PER_CLIENT = args.msgs
MSG_SIZE = args.size
CONNECT_TIMEOUT = args.connect_timeout
IO_TIMEOUT = args.io_timeout

latencies = []
errors = []
lock = threading.Lock()

def client_worker(cid):
    try:
        s = socket.create_connection((HOST, PORT), timeout=CONNECT_TIMEOUT)
        s.settimeout(IO_TIMEOUT)
        payload = (f"client{cid}-" + "X"*(MSG_SIZE-10) + "\n").encode()
        for i in range(MSG_PER_CLIENT):
            t0 = time.monotonic()
            s.sendall(payload)
            # read until newline
            data = b""
            while not data.endswith(b"\n"):
                chunk = s.recv(8192)
                if not chunk:
                    raise RuntimeError("connection closed")
                data += chunk
            t1 = time.monotonic()
            with lock:
                latencies.append((t1 - t0))
        s.close()
    except Exception as e:
        with lock:
            errors.append((cid, str(e)))

def percentile(data, p):
    if not data: return None
    k = (len(data)-1) * (p/100.0)
    f = int(k)
    c = min(f+1, len(data)-1)
    if f == c:
        return data[int(k)]
    d0 = data[f] * (c-k)
    d1 = data[c] * (k-f)
    return d0 + d1

def main():
    start = time.time()
    threads = []
    for i in range(NUM_CLIENTS):
        t = threading.Thread(target=client_worker, args=(i,))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    end = time.time()
    total_msgs = len(latencies)
    duration = end - start
    ops_per_sec = total_msgs / duration if duration > 0 else 0

    lat_ms = [x*1000.0 for x in latencies]
    lat_ms.sort()

    print(f"Stress test finished: clients={NUM_CLIENTS} msgs/client={MSG_PER_CLIENT} msg_size={MSG_SIZE}")
    print(f"Total messages: {total_msgs} errors: {len(errors)} duration={duration:.2f}s ops/s={ops_per_sec:.2f}")
    if lat_ms:
        print(f"Latency ms: mean={statistics.mean(lat_ms):.2f} p50={percentile(lat_ms,50):.2f} p95={percentile(lat_ms,95):.2f} p99={percentile(lat_ms,99):.2f} max={max(lat_ms):.2f}")
    if errors:
        print("Some errors (sample up to 10):")
        for e in errors[:10]:
            print(e)

if __name__ == '__main__':
    main()
