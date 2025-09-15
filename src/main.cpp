
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>


// 设置非阻塞
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
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

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
    ev.events = EPOLLIN | EPOLLET; // 读事件 + ET模式
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "epoll_ctl failed\n";
        return -1;
    }

    std::cout << "Server is running on port 8080 (Epoll ET)...\n";

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd) {
                // 处理新连接
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd == -1) {
                        break;
                    }
                    setNonBlocking(client_fd);
                    epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
                    std::cout << "New connection accepted, fd=" << client_fd << std::endl;
                }
            } else if (events[i].events & EPOLLIN) {
                // 处理数据读入
                int client_fd = events[i].data.fd;
                char buf[4096];
                while (true) {
                    ssize_t n = read(client_fd, buf, sizeof(buf));
                    if (n > 0) {
                        std::cout << "Received from fd=" << client_fd << ": " << std::string(buf, n) << std::endl;
                        // 回显
                        write(client_fd, buf, n);
                    } else if (n == 0) {
                        std::cout << "Client fd=" << client_fd << " disconnected" << std::endl;
                        close(client_fd);
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        std::cerr << "Read error on fd=" << client_fd << std::endl;
                        close(client_fd);
                        break;
                    }
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}