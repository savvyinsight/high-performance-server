# High Performance Server

[中文](README_zh.md)

A lightweight, educational high-concurrency TCP echo server implemented in modern C++ (C++11) for Linux. The project demonstrates a production-inspired pattern using:

- epoll in edge-triggered (ET) mode
- a fixed-size thread pool for request handling
- per-connection state objects
- a timer manager (min-heap) for idle connection cleanup
- a simple thread-safe logger

This repository is intended as a learning and experimentation platform — you can run the server, exercise it with the included stress tools, and iterate on tuning parameters.

**Key goals**
- Demonstrate high-concurrency server building blocks (socket, epoll, non-blocking IO)
- Show correct ET + non-blocking read loops and EPOLLONESHOT re-arm semantics
- Provide a simple framework for experiments and performance tuning

Repository layout (important files)
- `src/main.cpp` — server entry, epoll loop, accept + worker enqueue logic
- `include/Connection.h` — per-connection context (buffers, last-active timestamp)
- `include/ThreadPool.h` + `src/ThreadPool.cpp` — simple fixed worker pool
- `include/Timer.h` + `src/timer_manager.cpp` — min-heap timer manager to close idle connections
- `include/Logger.h` + `src/Logger.cpp` — small thread-safe logger writing to stdout and optional file
- `tests/smoke_test.py` — quick correctness smoke test
- `tests/stress_test.py` — multithreaded TCP stress test that measures ops/s and latency
- `scripts/run_experiments.sh` — wrapper to run stress experiments across thread-pool sizes
- `CMakeLists.txt` — build configuration

Build
```
mkdir -p build && cd build
cmake ..
make -j
```

Run (simple)
```
./high_performance_server [--threads N]

# defaults to 4 worker threads; you can pass a number or `--threads N`.
```

Run smoke test
```
python3 tests/smoke_test.py
```

Run stress test (example)
```
python3 tests/stress_test.py --clients 200 --msgs 200 --size 256
```

Automated experiments
```
./scripts/run_experiments.sh experiments
```

The script will:

Note: The wrapper can optionally try to apply some `sysctl` tuning (set `APPLY_SYSCTL=1` inside the script) but that requires `sudo` and will modify system settings.

Aggregating experiment outputs
- After running `scripts/run_experiments.sh`, collect and compare results with the aggregator:

```sh
python3 scripts/aggregate_results.py --dir experiments --out-csv experiments/summary.csv --out-json experiments/summary.json
```

This produces `summary.csv` and `summary.json` summarizing `clients`, `msgs_per_client`, `msg_size`, `total_messages`, `errors`, `duration_s`, `ops_per_s`, and latency percentiles in ms.

Benchmark (QPS) — quick summary
--------------------------------
The `experiments_quick/summary.csv` produced by the wrapper contains example QPS (ops/s) numbers collected on the developer machine using `tests/stress_test.py` with `--clients 100 --msgs 100 --size 256` (one-shot Python tester). Example results:

 - threads=1  : ops/s = 0.00   (100 errors; tester could not complete under this config)
 - threads=2  : ops/s = 2564.44, errors=0
 - threads=4  : ops/s = 2695.89, errors=0
 - threads=8  : ops/s = 2368.79, errors=9
 - threads=16 : ops/s = 2233.35, errors=19

Interpretation:
- The best QPS in this quick sweep was observed at 4 worker threads (~2696 ops/s) on this machine and test harness. Higher thread counts produced slightly lower QPS and more errors in this run — likely due to client-side limits, kernel backlog, or contention on shared resources.
- These numbers are a baseline for local experiments only. For repeatable production-like benchmarking use a dedicated load generator (e.g., `wrk` for HTTP) on a separate machine and apply the kernel tuning described earlier.

If you want, I can run a controlled sweep with `ulimit`/`sysctl` tuning and a larger external load generator and add the results to a formal table in this README.

Logging
- The server writes human-readable log lines to stdout. Optionally the code can also write to `server.log` (see `Logger::init("server.log")` in `main.cpp`).

How the server works (short technical overview)
- The listening socket is non-blocking and registered with epoll in ET mode.
- On incoming connections the server sets each client socket to non-blocking and registers it with `EPOLLIN | EPOLLET | EPOLLONESHOT` so a single worker thread handles the socket at a time.
- When epoll signals readability, the main thread enqueues a task into the thread pool. The worker reads in a loop until `EAGAIN`/`EWOULDBLOCK` (standard ET pattern), echoes data back, updates the connection's last-active timestamp, and calls the timer manager to refresh the timeout.
- On drain (`EAGAIN`) the worker re-arms the socket with `epoll_ctl(EPOLL_CTL_MOD, ..., EPOLLONESHOT)` to receive the next event.
- The timer manager runs in the background and closes sockets that were idle past the configured timeout; it verifies the connection's last-active timestamp to avoid races with refreshed timers.

Configuration and tuning (practical tips)
- Thread pool: tune worker count to match your CPU and workload (e.g., `num_cores * 2` is a reasonable starting point for IO-bound workloads).
- File descriptors: raise `ulimit -n` before high-concurrency tests:
  - `ulimit -n 100000`
- Kernel networking knobs useful for stress testing:
  - `sudo sysctl -w net.core.somaxconn=10240`
  - `sudo sysctl -w net.ipv4.tcp_tw_reuse=1`
  - `sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"`
- Backlog: the `listen()` backlog argument and `somaxconn` can limit the rate of accepted connections; increase both when testing large bursts.

Stress testing guidance
- The included `tests/stress_test.py` is useful for raw TCP echo tests and reports latency percentiles.
- For HTTP workloads use tools like `wrk` or `wrk2` for more reliable steady-state measurements.
- If you see many connection errors (timeouts/refused), first raise `ulimit -n` and tune `somaxconn`.

Known limitations and safety notes
- This is an educational project, not a hardened production server. It focuses on showing patterns rather than providing full production features (authentication, TLS, resource isolation, comprehensive error handling).
- Logging is synchronous and minimal. For high-throughput production work, consider async logging and log rotation.