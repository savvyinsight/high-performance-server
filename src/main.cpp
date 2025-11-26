#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include <signal.h>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "../include/ThreadPool.h"
#include "../include/Connection.h"
#include "../include/Timer.h"
#include "../include/Logger.h"


//set non-blocking
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static volatile sig_atomic_t stop_flag = 0;

static void handle_signal(int) {
    stop_flag = 1;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        Logger::instance().error("Socket creation failed");
        return -1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;//Listen on all IPs
    address.sin_port = htons(8080);//host to network byte order

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        Logger::instance().error("Bind failed");
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        Logger::instance().error("Listen failed");
        return -1;
    }

    setNonBlocking(server_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        Logger::instance().error("epoll_create1 failed");
        return -1;
    }

    epoll_event ev, events[1024];
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "epoll_ctl failed\n";
        return -1;
    }

    // initialize logger file output (optional)
    Logger::instance().init("server.log");

    // register simple signal handlers for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create a thread pool, assuming 4 worker threads (inside main)
    ThreadPool pool(4);

    // per-connection map: fd -> Connection
    // protected by connections_mutex because both main thread and workers
    // may insert/erase entries.
    std::unordered_map<int, std::shared_ptr<Connection>> connections;
    std::mutex connections_mutex;

    // Timer manager: closes idle connections after timeout seconds
    TimerManager timer(epoll_fd, connections, connections_mutex, 60); // 60s default
    timer.start();

    Logger::instance().info("Server is running on port 8080 (Epoll ET)...");

    while (!stop_flag) {
        // wake periodically to check stop flag
        int nfds = epoll_wait(epoll_fd, events, 1024, 1000);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            Logger::instance().error(std::string("epoll_wait failed errno=") + std::to_string(errno));
            break;
        }
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd) {
                //process new connection
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd == -1) {
                        break;
                    }
                    setNonBlocking(client_fd);
                    epoll_event client_ev;
                    // 使用 EPOLLONESHOT 确保每次只有一个线程处理该 fd
                    client_ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                    client_ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);

                    // create Connection object and insert into map
                    {
                        auto conn = std::make_shared<Connection>(client_fd);
                        std::lock_guard<std::mutex> lock(connections_mutex);
                        connections[client_fd] = conn;
                    }

                    // schedule initial timer for this connection
                    timer.add_or_refresh(client_fd, 60);

                    Logger::instance().info(std::string("New connection accepted, fd=") + std::to_string(client_fd));
                }
            } else if (events[i].events & EPOLLIN) {
                int client_fd = events[i].data.fd;

                // lookup the Connection object for this fd
                std::shared_ptr<Connection> conn;
                {
                    std::lock_guard<std::mutex> lock(connections_mutex);
                    auto it = connections.find(client_fd);
                    if (it != connections.end()) conn = it->second;
                }

                if (!conn) {
                    // Connection not found; it may have been closed concurrently
                    continue;
                }

                // enqueue a task that holds a shared_ptr to the Connection
                // so the Connection object stays alive while the worker runs.
                pool.enqueue([epoll_fd, client_fd, conn, &connections, &connections_mutex, &timer]() {
                    char buf[4096];
                    while (true) {
                        ssize_t n = read(client_fd, buf, sizeof(buf));
                        if (n > 0) {
                            // update last-active timestamp
                            conn->touch();
                            // refresh timer because we received activity
                            timer.add_or_refresh(client_fd, 60);
                            // perform simple echo logic; protect out_buffer if necessary
                            Logger::instance().debug(std::string("[Worker] Received from fd=") + std::to_string(client_fd) + ": " + std::string(buf, n));
                            ssize_t w = write(client_fd, buf, n);
                            (void)w; // ignore short writes for this simple example
                        } else if (n == 0) {
                            // orderly shutdown by peer
                            Logger::instance().info(std::string("[Worker] Client fd=") + std::to_string(client_fd) + " disconnected");
                            close(client_fd);
                            // remove from connections map
                            {
                                std::lock_guard<std::mutex> lock(connections_mutex);
                                connections.erase(client_fd);
                            }
                            break;
                        } else {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // drained all data for ET + non-blocking; re-arm EPOLLONESHOT
                                epoll_event ev_mod;
                                ev_mod.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                                ev_mod.data.fd = client_fd;
                                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_mod) == -1) {
                                    // if re-arm fails, clean up: socket may be closed
                                    Logger::instance().error(std::string("epoll_ctl MOD failed for fd=") + std::to_string(client_fd) + ", errno=" + std::to_string(errno));
                                    close(client_fd);
                                    std::lock_guard<std::mutex> lock(connections_mutex);
                                    connections.erase(client_fd);
                                }
                                break;
                            }
                            Logger::instance().error(std::string("[Worker] Read error on fd=") + std::to_string(client_fd));
                            close(client_fd);
                            {
                                std::lock_guard<std::mutex> lock(connections_mutex);
                                connections.erase(client_fd);
                            }
                            break;
                        }
                    }
                });
            }
        }
    }
    // graceful shutdown: stop timer, close connections and fds
    Logger::instance().info("Shutting down server...");
    timer.stop();

    {
        std::lock_guard<std::mutex> lk(connections_mutex);
        for (auto &p : connections) {
            int fd = p.first;
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            p.second->closed = true;
        }
        connections.clear();
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}