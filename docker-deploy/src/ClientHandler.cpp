#include "ClientHandler.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <exception>

ClientHandler::ClientHandler() {

}

ClientHandler::~ClientHandler() {

}

void ClientHandler::handle_client_requests(int client_sockfd) {
    char buffer[8192] = {0};
    while (true){
    int bytes_read = recv(client_sockfd, buffer, sizeof(buffer), 0);
    
    if (bytes_read < 0) {
        std::cerr << "Failed to read request from client\n";
        return;
    } else if (bytes_read == 0) {
        std::cout << "Connection closed" << std::endl;
        return;
    }

    HttpRequest request;
    if (!request.parse_request(std::string(buffer))) {
        std::cerr << "Malformed request, sending 400 Bad Request\n";
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_socket, response.c_str(), response.length(), 0);
        return;
    }

    RequestHandler handler(cache);
    handler.handle_request(request, client_socket);
}

}