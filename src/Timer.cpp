#include "../include/Timer.h"
#include "../include/Connection.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

TimerManager::TimerManager(int epoll_fd,
                           std::unordered_map<int, std::shared_ptr<Connection>> &conns,
                           std::mutex &conns_mtx,
                           int default_timeout_sec)
    : epoll_fd_(epoll_fd), conns_(conns), conns_mtx_(conns_mtx),
      default_timeout_sec_(default_timeout_sec), running_(false) {}

TimerManager::~TimerManager() { stop(); }

void TimerManager::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&TimerManager::run_loop, this);
}

void TimerManager::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void TimerManager::add_or_refresh(int fd, int timeout_sec) {
    int t = timeout_sec > 0 ? timeout_sec : default_timeout_sec_;
    Item it{Clock::now() + std::chrono::seconds(t), fd};
    {
        std::lock_guard<std::mutex> lk(pq_mtx_);
        pq_.push(std::move(it));
    }
}

void TimerManager::run_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = Clock::now();

        while (true) {
            Item it;
            {
                std::lock_guard<std::mutex> lk(pq_mtx_);
                if (pq_.empty()) break;
                it = pq_.top();
                if (it.expire > now) break;
                pq_.pop();
            }

            // Find connection; skip if not present
            std::shared_ptr<Connection> conn;
            {
                std::lock_guard<std::mutex> lk(conns_mtx_);
                auto itc = conns_.find(it.fd);
                if (itc == conns_.end()) continue;
                conn = itc->second;
            }

            // Compare last_active under conn->mtx to avoid races
            bool should_close = false;
            {
                std::lock_guard<std::mutex> lk(conn->mtx);
                auto last = conn->last_active;
                if (last + std::chrono::seconds(default_timeout_sec_) <= now) {
                    should_close = true;
                }
            }

            if (!should_close) continue; // was refreshed

            // close connection: remove from epoll and from map
            {
                std::lock_guard<std::mutex> lk(conns_mtx_);
                auto itc2 = conns_.find(it.fd);
                if (itc2 != conns_.end()) {
                    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it.fd, nullptr);
                    close(it.fd);
                    itc2->second->closed = true;
                    conns_.erase(itc2);
                    std::cout << "[Timer] Closed idle fd=" << it.fd << std::endl;
                }
            }
        }
    }
}
