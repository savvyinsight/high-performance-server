#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "../include/ThreadPool.h"
#include "../include/Connection.h"


//set non-blocking
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;//Listen on all IPs
    address.sin_port = htons(8080);//host to network byte order

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed\n";
        return -1;
    }

    setNonBlocking(server_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "epoll_create1 failed\n";
        return -1;
    }

    epoll_event ev, events[1024];
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "epoll_ctl failed\n";
        return -1;
    }

    // Create a thread pool, assuming 4 worker threads (inside main)
    ThreadPool pool(4);

    // per-connection map: fd -> Connection
    // protected by connections_mutex because both main thread and workers
    // may insert/erase entries.
    std::unordered_map<int, std::shared_ptr<Connection>> connections;
    std::mutex connections_mutex;

    std::cout << "Server is running on port 8080 (Epoll ET)...\n";

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);
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

                    std::cout << "New connection accepted, fd=" << client_fd << std::endl;
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
                pool.enqueue([epoll_fd, client_fd, conn, &connections, &connections_mutex]() {
                    char buf[4096];
                    while (true) {
                        ssize_t n = read(client_fd, buf, sizeof(buf));
                        if (n > 0) {
                            // update last-active timestamp
                            conn->touch();
                            // perform simple echo logic; protect out_buffer if necessary
                            std::cout << "[Worker] Received from fd=" << client_fd << ": " << std::string(buf, n) << std::endl;
                            ssize_t w = write(client_fd, buf, n);
                            (void)w; // ignore short writes for this simple example
                        } else if (n == 0) {
                            // orderly shutdown by peer
                            std::cout << "[Worker] Client fd=" << client_fd << " disconnected" << std::endl;
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
                                    std::cerr << "epoll_ctl MOD failed for fd=" << client_fd << ", errno=" << errno << std::endl;
                                    close(client_fd);
                                    std::lock_guard<std::mutex> lock(connections_mutex);
                                    connections.erase(client_fd);
                                }
                                break;
                            }
                            std::cerr << "[Worker] Read error on fd=" << client_fd << std::endl;
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

    close(server_fd);
    close(epoll_fd);
    return 0;
}