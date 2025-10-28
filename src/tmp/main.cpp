//basic networking example in C++
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int main(){
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd == -1){
        std::cerr<<"socket error"<<std::endl;
        return -1;
    }

    
}
