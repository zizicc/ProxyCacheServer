#include "ClientHandler.h"

void ClientHandler::process_client_request(int client_socket, CacheManager& cache) {
    char buffer[8192] = {0};
    while (true){
    int bytes_read = read(client_socket, buffer, sizeof(buffer));
    
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