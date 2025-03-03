#include "RequestHandler.h"
#include "Logger.h"
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>

RequestHandler::RequestHandler(CacheManager& cache) : cache(cache) {}

//this function will also handle responding to malformed requests with error code.
//will return -1 if we should close socket connection to client after handling (right now just for malformed request). 
//returns 0 otherwise (keep connection open)
int RequestHandler::handle_request(HttpRequest& request, int client_socket, int request_id, const std::string& client_ip) {
    // Use shared Logger instance
    Logger& logger = Logger::get_instance();

    // Log initial request
    logger.log_request(request_id, request.get_method() + " " + request.get_url() + " " + request.get_http_version(), client_ip);

    // Handle malformed request
    if (request.client_error_code != 0) {
        std::string error_response = "HTTP/1.1 " + std::to_string(request.client_error_code) + " Bad Request\r\nConnection: close\r\n\r\n";
        send(client_socket, error_response.c_str(), error_response.length(), 0);
        logger.log_error(request_id, "Malformed request received, closing connection.");
        return -1;
    }

    std::string method = request.get_method();
    std::string url = request.get_url();

    // Handle GET request and caching
    if (method == "GET") {
        if (cache.is_in_cache(url)) {
            std::shared_ptr<HttpResponse> cached_response = cache.get_cached_response(url);
            if (cached_response) {
                if (request.has_header("If-Modified-Since") || request.has_header("If-None-Match")) {
                    logger.log_cache_status(request_id, "in cache, requires validation");
    
                    // Modify request to send conditional headers
                    request.add_header("If-Modified-Since", cached_response->get_header("Last-Modified"));
                    request.add_header("If-None-Match", cached_response->get_header("ETag"));
    
                    HttpResponse response = forward_request(request, request_id);
                    if (response.get_status_line().find("304 Not Modified") != std::string::npos) {
                        logger.log_response(request_id, "HTTP/1.1 304 Not Modified (Using cached copy)");
                        send(client_socket, cached_response->serialize().c_str(), cached_response->serialize().length(), 0);
                        return 0;
                    }
                } else {
                    logger.log_cache_status(request_id, "in cache, valid");
                    send(client_socket, cached_response->serialize().c_str(), cached_response->serialize().length(), 0);
                    logger.log_response(request_id, cached_response->get_status_line());
                    return 0;
                }
            }
        }
    }

    // Handle HTTPS (CONNECT)
    if (method == "CONNECT") {
        handle_connect(request, client_socket, request_id);
        return 0;
    }

    // Forward other requests (including POST)
    HttpResponse response = forward_request(request, request_id);
    std::string response_str = response.serialize();
    send(client_socket, response_str.c_str(), response_str.length(), 0);

    // Log response to client
    logger.log_response(request_id, response.get_status_line());

    // Cache only 200 OK GET responses
    if (method == "GET" && response.is_cacheable()) {
        cache.store_response(request_id, url, std::make_shared<HttpResponse>(response));
        logger.log_cache_status(request_id, "cached, expires at " + response.get_header("Expires"));
    }

    return 0;
}

// Forward request to origin server
HttpResponse RequestHandler::forward_request(HttpRequest& request, int request_id) {
    Logger& logger = Logger::get_instance();
    int sockfd;
    struct addrinfo hints{}, *res;
    std::string server = request.get_host();

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve server address
    if (getaddrinfo(server.c_str(), "80", &hints, &res) != 0) {
        logger.log_error(request_id, "Failed to resolve host: " + server);
        return HttpResponse(); // 502 Bad Gateway
    }

    // Create and connect socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0 || connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        logger.log_error(request_id, "Failed to connect to server: " + server);
        freeaddrinfo(res);
        return HttpResponse(); // 502 Bad Gateway
    }
    
    freeaddrinfo(res);


    // Log only the request line (first line of serialized request)
    std::istringstream request_stream(request.serialize());
    std::string request_line;
    std::getline(request_stream, request_line);

    logger.log_forward_request(request_id, request.get_method() + " " + request.get_url() + " " + request.get_http_version(), request.get_host());

    // Send request
    std::string request_str = request.serialize();
    send(sockfd, request_str.c_str(), request_str.length(), 0);

    // Receive response from server
    char buffer[8192];
    int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        logger.log_error(request_id, "Failed to receive response from server.");
        close(sockfd);
        return HttpResponse(); // 502 Bad Gateway
    }

    // Parse response
    HttpResponse response;
    response.parse_response(std::string(buffer, bytes_received));

    // Log received response
    logger.log_received_response(request_id, response.get_status_line(), server);

    close(sockfd);
    return response;
}

// Handle CONNECT request (HTTPS tunnel)
void RequestHandler::handle_connect(HttpRequest& request, int client_socket, int request_id) {
    Logger& logger = Logger::get_instance();
    int remote_socket;
    struct addrinfo hints{}, *res;
    std::string server = request.get_host();

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve server address
    if (getaddrinfo(server.c_str(), "443", &hints, &res) != 0) {
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_socket, error_response.c_str(), error_response.length(), 0);
        logger.log_error(request_id, "Failed to resolve HTTPS host: " + server);
        return;
    }

    remote_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (remote_socket < 0 || connect(remote_socket, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_socket, error_response.c_str(), error_response.length(), 0);
        logger.log_error(request_id, "Failed to establish HTTPS tunnel to " + server);
        return;
    }

    freeaddrinfo(res);

    // Send 200 OK to client for tunnel establishment
    std::string success_response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_socket, success_response.c_str(), success_response.length(), 0);

    // Tunnel communication
    fd_set read_fds;
    char buffer[8192];

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(remote_socket, &read_fds);
        int max_fd = std::max(client_socket, remote_socket) + 1;

        if (select(max_fd, &read_fds, nullptr, nullptr, nullptr) < 0) {
            break;
        }

        if (FD_ISSET(client_socket, &read_fds)) {
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            send(remote_socket, buffer, bytes_received, 0);
        }

        if (FD_ISSET(remote_socket, &read_fds)) {
            int bytes_received = recv(remote_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            send(client_socket, buffer, bytes_received, 0);
        }
    }

    close(remote_socket);
    logger.log_tunnel_closed(request_id);
}