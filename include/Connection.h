// Connection.h
// Simple per-connection context used by the server.
// This struct holds the socket fd, read/write buffers, last-active timestamp
// and a mutex to protect per-connection fields when accessed by multiple threads.

#pragma once

#include <string>
#include <mutex>
#include <chrono>

struct Connection {
    int fd; // socket file descriptor
    std::string in_buffer;  // data read from socket but not yet processed
    std::string out_buffer; // data to be written to socket
    std::mutex mtx;         // protects buffers and closed flag
    bool closed{false};     // whether socket has been closed
    // last active timestamp used by timer/idle detection
    std::chrono::steady_clock::time_point last_active;

    explicit Connection(int _fd)
        : fd(_fd), last_active(std::chrono::steady_clock::now()) {}

    // update last_active to now (call whenever data is received)
    void touch() {
        std::lock_guard<std::mutex> lock(mtx);
        last_active = std::chrono::steady_clock::now();
    }
};
