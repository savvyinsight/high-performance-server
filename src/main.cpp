#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include "../include/ThreadPool.h"


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
                    std::cout << "New connection accepted, fd=" << client_fd << std::endl;
                }
            } else if (events[i].events & EPOLLIN) {
                int client_fd = events[i].data.fd;
                // 投递到线程池处理
                // capture epoll_fd so worker can re-arm the fd after reading all available data
                pool.enqueue([epoll_fd, client_fd]() {
                    char buf[4096];
                    while (true) {
                        ssize_t n = read(client_fd, buf, sizeof(buf));
                        if (n > 0) {
                            std::cout << "[Worker] Received from fd=" << client_fd << ": " << std::string(buf, n) << std::endl;
                            write(client_fd, buf, n);
                        } else if (n == 0) {
                            std::cout << "[Worker] Client fd=" << client_fd << " disconnected" << std::endl;
                            // close fd; since it's closed it's removed from epoll automatically
                            close(client_fd);
                            break;
                        } else {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // 已经读完所有数据（ET + 非阻塞），需要重新激活 EPOLLONESHOT
                                epoll_event ev_mod;
                                ev_mod.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                                ev_mod.data.fd = client_fd;
                                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev_mod) == -1) {
                                    // 如果 re-arm 失败，可能是 fd 已被关闭或出错
                                    std::cerr << "epoll_ctl MOD failed for fd=" << client_fd << ", errno=" << errno << std::endl;
                                    // 尝试关闭以清理
                                    close(client_fd);
                                }
                                break;
                            }
                            std::cerr << "[Worker] Read error on fd=" << client_fd << std::endl;
                            // on other errors, close the socket
                            close(client_fd);
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