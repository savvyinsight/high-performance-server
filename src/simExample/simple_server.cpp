//basic networking example in C++
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int main(){
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd == -1){
        std::cerr<<"socket error"<<std::endl;
        return -1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;//listen all ip
    server_addr.sin_port = htons(8080);

    if(bind(listen_fd,(sockaddr*)&server_addr,sizeof(server_addr)) == -1){
        std::cerr<<"bind error\n";
        close(listen_fd);
        return -1;
    }

    if(listen(listen_fd,5) == -1){
        std::cerr<<"listen error\n";
        close(listen_fd);
        return -1;
    }

    std::cout<<"Server listening on port 8080 ...\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int conn_fd = accept(listen_fd,(sockaddr*)&client_addr,&client_len);
        if(conn_fd == -1){
            std::cerr<<"accept error\n";
            continue;
        }

        std::cout << "Client connected\n";
        
        //Handle multiple messages for this connection
        while (true) {
            char buffer[1024] = {0};
            int n = read(conn_fd, buffer, sizeof(buffer));
            if(n<=0){
                if(n==0){
                    std::cout<<"Client disconnected\n";
                }else{
                    std::cerr<<"read error\n";
                }
                break; 
            }
            std::cout << "Received: " << buffer<< std::endl;

            // Echo back the client
            if(write(conn_fd, buffer, n) == -1){
                std::cerr<<"write error\n";
                break;
            }
        }
        close(conn_fd);
        std::cout<<"Connection closed\n";
    }
    close(listen_fd);
    return 0;
}
/*
test:
A terminal:
mkdir build
cd build
cmake ..
make
./high_performance_server

B terminal:(use telnet or nc to test)
telnet 127.0.0.1 8080
or
nc  127.0.0.1 8080
*/ 