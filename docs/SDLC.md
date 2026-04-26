# High-Performance HTTP Server SDLC Document

## 1. Project Overview

### Vision
Build a high-performance HTTP server in C++ on Linux that demonstrates scalable non-blocking network I/O, Reactor architecture, thread pooling, and efficient connection timeout handling.

### Scope
- Implement a production-inspired HTTP/1.1 server capable of handling concurrent client connections.
- Use Linux `epoll` in Edge-Triggered (ET) mode for event multiplexing.
- Use a fixed-size thread pool to avoid repeated thread creation overhead.
- Provide idle connection cleanup using a min-heap timer manager.
- Provide structured logging and support stress testing with standard benchmarks.

## 2. Requirements

### 2.1 Functional Requirements
- Accept TCP connections on a configurable port.
- Parse incoming HTTP/1.1 requests.
- Support at least `GET` requests and keep-alive connections.
- Generate valid HTTP responses with status lines, headers, and bodies.
- Correctly handle partial reads and writes in non-blocking mode.
- Maintain connection state across `EPOLLONESHOT` re-arms.
- Respond with a simple static or generated payload for valid requests.

### 2.2 Non-Functional Requirements
- Use `epoll` in Edge-Triggered mode for efficient event dispatch.
- Minimize latency and maximize throughput under concurrent load.
- Avoid frequent creation/destruction of OS threads by reusing a worker thread pool.
- Close idle connections after a configurable timeout.
- Log server events at configurable verbosity levels.
- Be implemented in modern C++ (C++11 or later) with cross-platform Linux portability.

### 2.3 Constraints
- The implementation must run on Linux.
- The project must use low-level network APIs (`socket`, `bind`, `listen`, `accept`, `read`, `write`).
- `epoll` must be used for I/O multiplexing.
- The reactor must support ET mode with `EPOLLONESHOT` semantics for worker dispatch.
- The timeout manager must use a min-heap or priority queue to manage expirations.

### 2.4 Assumptions
- The server focuses on HTTP semantics and performance, not on complete HTTP feature parity.
- TLS, authentication, and advanced HTTP extensions are out of initial scope.
- Benchmarking may use `webbench`, `wrk`, or similar tools, but the implementation should support any standard HTTP load generator.

## 3. Architecture and Design

### 3.1 High-Level Architecture

- **Network Acceptor**: Listens for inbound TCP connections and sets client sockets to non-blocking mode.
- **Epoll Reactor**: Uses `epoll_wait` in ET mode to detect readiness on sockets.
- **Connection Manager**: Maintains per-connection state, buffers, parser progress, and last-activity timestamps.
- **Thread Pool**: Worker threads process I/O and HTTP requests without creating a new thread per connection.
- **HTTP Engine**: Parses requests and generates responses.
- **Timer Manager**: Uses a min-heap to expire idle connections.
- **Logger**: Records informational, warning, and error events.

### 3.2 Component Diagram

- `main.cpp`
  - Accepts sockets
  - Registers file descriptors with epoll
  - Dispatches ready events to the thread pool
- `ThreadPool`
  - Receives work items
  - Executes worker functions concurrently
- `Connection`
  - Stores socket descriptor
  - Holds read/write buffers
  - Tracks HTTP parser state
  - Tracks last activity time
- `HttpParser` (new)
  - Parses request line, headers, and body
  - Supports incremental parsing for partial reads
- `HttpResponse` (new)
  - Builds status line, headers, and body
  - Formats `Content-Length`, `Connection`, and other headers
- `TimerManager`
  - Manages timeouts in a priority queue
  - Closes idle connections safely
- `Logger`
  - Provides `debug`, `info`, `warn`, `error`

### 3.3 Data Flow

1. Accept loop receives new client socket.
2. New socket set to non-blocking and registered with epoll as `EPOLLIN | EPOLLET | EPOLLONESHOT`.
3. When epoll signals a socket, the reactor enqueues a worker task.
4. Worker reads available data in a loop until `EAGAIN`.
5. Worker advances the HTTP parser.
6. If a complete request is available, the worker generates a response.
7. Worker writes as much as possible, handling partial writes.
8. If the connection remains open, the worker re-arms the socket with `EPOLL_CTL_MOD`.
9. TimerManager refreshes the connection's expiry on activity.
10. Idle connections are closed after timeout.

### 3.4 Detailed Design Decisions

#### 3.4.1 Epoll and Reactor Model
- Use a single main thread for accept and epoll dispatch.
- Use `EPOLLONESHOT` so a worker thread exclusively processes one connection at a time.
- Re-arm the fd only after the worker drains available data or finishes writing.

#### 3.4.2 HTTP Parsing
- Implement an incremental parser for HTTP request lines and headers.
- Support parsing across multiple read events.
- Validate requested methods and HTTP versions.
- Handle `Connection: keep-alive` and `Connection: close`.

#### 3.4.3 Request Handling
- Start with minimal `GET` support and a fixed response payload.
- Optionally extend to static file serving or simple dynamic routing.
- Ensure worker handles read/write in a non-blocking manner.

#### 3.4.4 Connection Timeout
- Store each connection's last active timestamp.
- Use a min-heap keyed by expiry time.
- On timer expiration, verify the connection's actual activity time before closing.

#### 3.4.5 Thread Pool
- Create a fixed number of worker threads at startup.
- Use a synchronized task queue with condition variable notification.
- Avoid creating or destroying threads during normal request handling.

#### 3.4.6 Logging
- Implement a singleton logger with thread-safe method calls.
- Allow optional log file output and standard output logging.
- Log key events such as new connections, errors, timeouts, and shutdown.

### 3.5 Non-Functional Architecture
- Use static configuration for port, thread count, timeouts, and log level.
- Maintain minimal dependencies to keep the code lightweight and easy to build with CMake.
- Design for incremental extension to HTTP/1.1 and future HTTP/2 enhancements.

## 4. Implementation Plan

### 4.1 Phase 1: Foundation and Refactor
- Audit the existing TCP echo server implementation.
- Refactor reusable modules: `ThreadPool`, `Logger`, `TimerManager`, `Connection`.
- Remove duplicated or stale files.
- Establish a clean CMake build with source under `src/`.

### 4.2 Phase 2: HTTP Layer
- Add `HttpParser` and `HttpResponse` abstractions.
- Implement request-line and header parsing.
- Add support for `GET` and keep-alive connection handling.
- Add response formatting helpers.

### 4.3 Phase 3: Reactor and Connection Handling
- Integrate HTTP parsing into the worker loop.
- Ensure the worker can handle partial reads, multiple requests per connection, and partial writes.
- Re-arm epoll correctly after each worker visit.
- Add handling for `Connection: close` and `keep-alive` semantics.

### 4.4 Phase 4: Timeout and Resource Cleanup
- Wire `TimerManager` into the connection lifecycle.
- Refresh timeouts on each request activity.
- Ensure idle connections are cleaned safely and concurrently.

### 4.5 Phase 5: Logging and Configuration
- Add log categories and log levels.
- Make port, thread count, timeouts, and log file optional runtime parameters.
- Add documentation for running and configuring the server.

### 4.6 Phase 6: Validation and Benchmarking
- Add smoke tests for HTTP request/response correctness.
- Add stress tests for concurrency and latency.
- Benchmark with `webbench`, `wrk`, or `wrk2` and document results.
- Create a reproducible `scripts/run_experiments.sh` workflow.

## 5. Milestones and Deliverables

### Milestone 1: Core Server
- Buildable C++ project.
- Single-threaded HTTP request handling.
- Valid HTTP responses for `GET`.
- Basic logging and configuration.

### Milestone 2: Concurrency and Reactor
- Epoll ET + Reactor architecture.
- Working thread pool.
- Partial read/write support.
- Keep-alive connections.

### Milestone 3: Stability and Scalability
- Timer-based idle connection cleanup.
- Stress-testing harness.
- Performance benchmark results.

### Milestone 4: Documentation
- Project requirements and design docs.
- Build and run instructions.
- Testing and benchmarking guides.

## 6. Quality Assurance

### Testing
- Unit tests for HTTP parsing and response formatting.
- Smoke tests for request/response correctness.
- Stress tests for concurrency and latency.
- Regression tests for connection timeouts.

### Performance Validation
- Use standard tools such as `webbench`, `wrk`, or `wrk2`.
- Measure throughput, latency, and error rate.
- Tune thread count, socket buffer sizes, and timeout settings.

### Code Review
- Confirm proper resource cleanup on shutdown.
- Review thread-safety for shared connection state.
- Validate epoll edge-triggered handling.

## 7. Risks and Mitigation

- **Incomplete HTTP support**: Start with minimal `GET` support and add features iteratively.
- **ET mode complexity**: Use `EPOLLONESHOT` plus worker-managed re-arm logic.
- **Partial I/O handling**: Design the worker loop to drain reads and resume writes.
- **Idle connection leaks**: Verify the timer manager with active refresh checks.

## 8. Future Enhancements
- Add full HTTP/1.1 header parsing and support for `POST`.
- Add static file serving with `sendfile` and content-type detection.
- Add TLS support via an SSL/TLS abstraction.
- Add HTTP/2 support in a future architectural iteration.
- Add metrics and observability endpoints.
