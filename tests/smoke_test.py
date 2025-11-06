#!/usr/bin/env python3
"""
Simple concurrent smoke test:
- opens N clients
- each client sends M messages and expects the same data echoed
- prints summary
"""
import socket, threading, time

HOST = "127.0.0.1"
PORT = 8080
NUM_CLIENTS = 10
MSG_PER_CLIENT = 5
TIMEOUT = 3.0

results = []
lock = threading.Lock()

def client_worker(id):
    try:
        s = socket.create_connection((HOST, PORT), timeout=TIMEOUT)
        s.settimeout(TIMEOUT)
        for i in range(MSG_PER_CLIENT):
            msg = f"client{id}-msg{i}\n".encode()
            s.sendall(msg)
            data = b""
            # read echo (may read partial; loop until we get newline)
            while not data.endswith(b"\n"):
                chunk = s.recv(4096)
                if not chunk:
                    break
                data += chunk
            ok = (data == msg)
            with lock:
                results.append((id, i, ok, data.decode(errors="replace")))
            time.sleep(0.05)
        s.close()
    except Exception as e:
        with lock:
            results.append((id, 'ERR', False, str(e)))

threads = []
for i in range(NUM_CLIENTS):
    t = threading.Thread(target=client_worker, args=(i,))
    t.start()
    threads.append(t)

for t in threads:
    t.join()

# summary
ok_count = sum(1 for r in results if r[2])
total = len(results)
print(f"Total ops: {total}, OK: {ok_count}, Failed: {total - ok_count}")
for r in results:
    if not r[2]:
        print("FAILED:", r)