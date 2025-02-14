#include <iostream>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#define PROXY_SERVER_PORT 80


int main() {
    std::cout << "Proxy server running..." << std::endl;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "Failed to create socket" << std::endl;
        return 0;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; //binds socket to all available interfaces (ip addresses), not just localhost. this allows it to accept connections
    address.sin_port = htons(PROXY_SERVER_PORT);

    if (bind(sockfd, (sockaddr*)&address, sizeof(address)) < 0) { //bind socket to port PROXY_SERVER_PORT
        std::cout << "Failed to bind socket to port" << std::endl;
        return 0;
    }

    if (listen(sockfd, 10) < 0) { //TODO: not really sure what to set n to here, just set to 10
        std::cout << "Failed to set up socket to listen for incoming connections" << std::endl;
        return 0;
    }

    std::cout << "Proxy server listening on port 80" << std::endl;
    
    while (true) { //do stuff with accepting connection requests here
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }

    close(sockfd); //TODO: not really RAII, should attach sockfd to some proxy server object so 
                   //if the program exits early and object is destructed the sockfd is released

    return 0;
}