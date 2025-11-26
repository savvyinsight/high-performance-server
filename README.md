# 🚀 High Performance Server

<div align="center">

**Language** | [English](README.md) | [中文](README_zh.md) | 
Project Development Roadmap  

**Basic Network Communication Module (Socket + TCP)**  
Objective: Enable the server to accept client connections and send/receive data.  
Technical Points: C++11, Linux Socket API (socket, bind, listen, accept, recv, send).  

**Epoll I/O Multiplexing (Reactor Model, ET Mode)**  
Objective: Use Epoll to manage multiple connections and achieve high concurrency.  
Technical Points: epoll_create, epoll_ctl, epoll_wait, ET mode.  

**Thread Pool**  
Objective: Use a thread pool to handle requests and avoid frequent thread creation/destruction.  
Technical Points: C++11 thread library (std::thread, std::mutex, std::condition_variable), task queue.  

**Timer (Min-Heap)**  
Objective: Manage inactive connections and close them periodically.  
Technical Points: Priority queue (std::priority_queue), time management.  

**Log System**  
Objective: Record server runtime status for easier debugging.  
Technical Points: File I/O, log formatting.  

**Stress Testing and Optimization**  
Objective: Use tools like WebBench to test performance and analyze bottlenecks.

## Stress Test

This repo includes a simple Python stress test at `tests/stress_test.py` that opens many concurrent clients, sends messages and reports throughput (ops/s) and latency percentiles (p50/p95/p99).

Recommended external tools:
- `wrk` (https://github.com/wg/wrk) — a modern HTTP benchmarking tool (for HTTP workloads).
- `webbench` — lightweight older tool useful for simple TCP/HTTP tests.

Kernel / OS tuning tips for high-concurrency tests:
- Increase maximum open files (per-process): `ulimit -n 100000`
- Increase system backlog / somaxconn: `sudo sysctl -w net.core.somaxconn=10240`
- Enable reuse of TIME_WAIT sockets: `sudo sysctl -w net.ipv4.tcp_tw_reuse=1`
- Increase ephemeral ports if needed: `sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"`

Quick commands to run a stress test with `wrk` (if server speaks HTTP):
```sh
# example: 4 threads, 100 connections, 30s test
wrk -t4 -c100 -d30s http://127.0.0.1:8080/
```

To run the included Python stress test (no external tools required):
```sh
# build and run server in one terminal
mkdir -p build && cd build
cmake .. && make -j
./high_performance_server

# in another terminal, run the stress test
python3 tests/stress_test.py
```

Recorded baseline (example run on developer machine):
- Parameters: `NUM_CLIENTS=200`, `MSG_PER_CLIENT=200`, `MSG_SIZE=256`
- Results (quick run on this machine): `Total messages: 20400, errors: 98, duration=10.86s, ops/s=1877.61`
- Latency (this run): `mean=52.25ms p50=20.24ms p95=180.20ms p99=305.59ms max=1342.18ms`

Measured notes & troubleshooting:
- The Python tester opens many concurrent TCP connections from one client runner; default OS limits (open files, connection backlog) can cause timed-out connections — you may see `timed out` errors in the tester.
- To reduce errors and reach higher ops/s, try the following on the host before running a large stress test:
	- `ulimit -n 100000` (raise per-process file descriptor limit)
	- `sudo sysctl -w net.core.somaxconn=10240`
	- `sudo sysctl -w net.ipv4.tcp_tw_reuse=1`
	- increase `ThreadPool` size in `src/main.cpp` (e.g., to number of CPU cores * 2) to better use workers for concurrent reads/writes.

If you want, I can run another controlled experiment after you tell me which tuning knobs you'd like to change (e.g., increase `ulimit -n` and `somaxconn`, or increase thread-pool size) and I'll add the results to this README.

Notes on interpretation:
- The included Python tester is a guidance tool — `wrk`/`wrk2` is better for steady-state measurement under HTTP workloads. For raw TCP echo testing the Python script gives a quick measurable baseline.
- If you see many connection errors, raise `ulimit -n` and tune `somaxconn` and `backlog` in `listen()` (default backlog might be small on some systems).
</div>

## Status and How to run

- **Implemented:** Basic TCP socket server, Epoll (ET), ThreadPool, Timer (min-heap), simple Logger (file + stdout).
- **To build:**

```sh
mkdir -p build && cd build
cmake ..
make -j
```

- **Run server:**

```sh
./high_performance_server
```

- **Smoke test:**

```sh
python3 tests/smoke_test.py
```

