# 高性能服务器项目

## 项目简介
本项目旨在实现一个基于C++11的高并发服务器，采用Linux Socket编程、线程池、Epoll（ET模式）等技术，深入理解网络编程与多线程核心技术。

## 技术栈
- C++11
- Linux Socket
- Epoll（ET模式）
- 线程池

## 预期目标
- 支持高并发连接
- 实现Reactor模型
- 支持定时器处理非活动连接
- 日志系统
- 性能测试与优化

## 目录结构
high-performance-server/
├── src/
│   └── main.cpp
├── include/
├── CMakeLists.txt
└── README.md