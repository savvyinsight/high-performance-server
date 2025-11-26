# 高性能服务器项目

## 项目简介
## 技术栈
C++11
Linux Socket
Epoll（ET模式）
线程池

## 主要特性
- epoll (ET) + non-blocking socket
- EPOLLONESHOT + re-arm 模式，避免多个线程同时处理同一 fd
- 线程池处理读写任务，避免频繁创建/销毁线程
- 定时器用于清理闲置连接
- 提供用于压力测试的 Python 脚本和实验封装脚本

## 项目结构（重要文件）
- `src/main.cpp` — 服务器入口，epoll 循环，accept 与任务下发逻辑
- `include/Connection.h` — 连接上下文（缓冲区、活动时间戳）
- `include/ThreadPool.h`, `src/ThreadPool.cpp` — 简单线程池实现
- `include/Timer.h`, `src/timer_manager.cpp` — 最小堆定时器
- `include/Logger.h`, `src/Logger.cpp` — 简单线程安全日志
- `tests/smoke_test.py` — 正确性 smoke test
- `tests/stress_test.py` — 压力测试脚本（多线程）
- `scripts/run_experiments.sh` — 自动化实验脚本

## 构建
```sh
mkdir -p build && cd build
cmake ..
make -j
```

## 运行
```
./high_performance_server [--threads N]

# 默认 4 个工作线程；可以通过传入数字或 `--threads N` 指定。
```

## 测试
- Smoke 测试：
```sh
python3 tests/smoke_test.py
```

- 压力测试（示例）：
```sh
python3 tests/stress_test.py --clients 200 --msgs 200 --size 256
```

- 自动化实验：
```sh
./scripts/run_experiments.sh experiments
```

## 基准（QPS）快速汇总
下面的结果来自 `experiments_quick/summary.csv`，当时使用的参数为：`tests/stress_test.py --clients 100 --msgs 100 --size 256`（本地 Python 压力测试器）：

- threads=1  : QPS = 0.00   （100 个错误，测试未能完成）
- threads=2  : QPS = 2564.44，错误=0
- threads=4  : QPS = 2695.89，错误=0
- threads=8  : QPS = 2368.79，错误=9
- threads=16 : QPS = 2233.35，错误=19

说明:
- 最佳 QPS 在本次快速测量中出现在 4 个工作线程（约 2696 QPS）。
- 线程数过多时，QPS 反而下降并伴随更多错误，这通常由客户端负载生成瓶颈、内核监听队列（backlog）、文件描述符限制或竞态导致。
- 测试环境（脚本参数、CPU、内核参数、`ulimit`），这些结果是本地基线，可通过独立负载机与 `wrk`/`wrk2` 得到更稳定、可复现的对比数据。

如何更严谨地获取 QPS:
- 在单独的机器上运行成熟的负载工具（例如 `wrk`、`wrk2` 或自定义 C/Go 负载发生器）。
- 在每次实验前设置 `ulimit -n`、调整 `net.core.somaxconn` 并记录所有系统变量。把每次实验输出通过 `scripts/aggregate_results.py` 聚合为 CSV，便于展示趋势。


## 性能调优（建议）
- 提高进程最大文件描述符：`ulimit -n 100000`
- 调整内核参数：
	- `sudo sysctl -w net.core.somaxconn=10240`
	- `sudo sysctl -w net.ipv4.tcp_tw_reuse=1`
	- `sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"`
- 线程池大小：根据 CPU 与 IO 特性调整（`num_cores * 2` 为常见起点）

## 说明与限制
- 本项目是教学/实验用途，不是面向生产的完整服务器实现。
- 日志为同步输出；高吞吐场景建议使用异步日志与日志轮替方案。
```
│   └── main.cpp
├── include/
├── CMakeLists.txt
└── README.md