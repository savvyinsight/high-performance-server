cmake_minimum_required(VERSION 3.10)
project(high_performance_server)

set(CMAKE_CXX_STANDARD 11)

add_executable(high_performance_server src/main.cpp src/ThreadPool.cpp src/timer_manager.cpp src/Logger.cpp)