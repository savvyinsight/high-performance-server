#pragma once

#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <memory>

struct Connection; // forward declaration

// TimerManager: manages idle connection timeouts using a min-heap.
// - It holds a reference to the connections map and its mutex so it can
//   safely remove idle connections.
// - It runs a background thread that periodically checks the heap and
//   closes connections that have been idle longer than the timeout.
class TimerManager {
public:
    using Clock = std::chrono::steady_clock;

    explicit TimerManager(int epoll_fd,
                          std::unordered_map<int, std::shared_ptr<Connection>> &conns,
                          std::mutex &conns_mtx,
                          int default_timeout_sec = 60);
    ~TimerManager();

    // start/stop the background thread
    void start();
    void stop();

    // schedule or refresh timeout for fd (expires after timeout_sec seconds)
    void add_or_refresh(int fd, int timeout_sec = -1);

private:
    struct Item {
        Clock::time_point expire;
        int fd;
        bool operator>(Item const &o) const { return expire > o.expire; }
    };

    int epoll_fd_;
    std::unordered_map<int, std::shared_ptr<Connection>> &conns_;
    std::mutex &conns_mtx_;
    int default_timeout_sec_;

    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq_;
    std::mutex pq_mtx_;

    std::thread worker_;
    std::atomic<bool> running_;

    void run_loop();
};
