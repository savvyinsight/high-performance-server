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

