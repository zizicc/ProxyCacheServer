#include "RequestHandler.h"
#include "Logger.h"
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>

//returns 0 on success, -1 on error
int RequestHandler::reliable_send(int sockfd, const char* message, size_t len, int request_id) {
    int remaining = len;
    int sent;
    const char* curr_ptr = message;

    Logger& logger = Logger::get_instance();

    while (remaining > 0) {
        sent = send(sockfd, curr_ptr, remaining, 0);

        if (sent < 0) {
            logger.log_error(request_id, "A send returned -1, closing connection.");
            return -1;
        }

        remaining -= sent;
        curr_ptr += sent;
    }

    return 0;
}

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
        logger.log_error(request_id, "Malformed request received, closing connection.");

        std::string error_response = "HTTP/1.1 " + std::to_string(request.client_error_code) + " Bad Request\r\nConnection: close\r\n\r\n";
        reliable_send(client_socket, error_response.c_str(), error_response.length(), request_id); //dont need result, closing regardless
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
                        if (reliable_send(client_socket, cached_response->serialize().c_str(), cached_response->serialize().length(), request_id) < 0) {
                            return -1;
                        }

                        return 0;
                    } //DO WE NEED AN ELSE??
                } else {
                    logger.log_cache_status(request_id, "in cache, valid");
                    if (reliable_send(client_socket, cached_response->serialize().c_str(), cached_response->serialize().length(), request_id) < 0) {
                        return -1;
                    }
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
    if (reliable_send(client_socket, response_str.c_str(), response_str.length(), request_id) < 0) {
        return -1;
    }

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

    struct addrinfo* it = NULL;
    for (it = res; it != NULL; it = it->ai_next) {
        sockfd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sockfd == -1) //failed socket creation using current address structure, try next one
            continue;

        if (connect(sockfd, it->ai_addr, it->ai_addrlen) != -1) //successful connection to ringmaster
            break;

        close(sockfd); //failed connect using current address structure, try next one
    }

    freeaddrinfo(res);

    if (it == NULL) { //tried all addresses, none succeeded
        logger.log_error(request_id, "Failed to connect to server: " + server);
        return HttpResponse(); // 502 Bad Gateway
    }


    // Log only the request line (first line of serialized request)
    // std::istringstream request_stream(request.serialize());
    // std::string request_line;
    // std::getline(request_stream, request_line);

    logger.log_forward_request(request_id, request.get_method() + " " + request.get_url() + " " + request.get_http_version(), request.get_host());

    //Send request
    std::string request_str = request.serialize();
    if (reliable_send(sockfd, request_str.c_str(), request_str.length(), request_id) < 0) {
        logger.log_error(request_id, "Failed to send data to server.");
        close(sockfd);
        return HttpResponse(); // 502 Bad Gateway
    }

    //Receive response from server
    int buffer_read_size = 8192;
    char buffer[buffer_read_size];
    std::string curr_message;
    HttpResponse response;

    while (true) {
        int bytes_read = recv(sockfd, buffer, buffer_read_size, 0);

        if (bytes_read < 0) {
            logger.log_error(request_id, "Failed to receive response from server.");
            close(sockfd);
            return HttpResponse(); //502 Bad Gateway
        } else if (bytes_read == 0) { //server has closed connection and on last iteration we did not have full http response
            logger.log_error(request_id, "Failed to receive response from server.");
            close(sockfd);
            return HttpResponse(); //502 Bad Gateway
        }

        curr_message += std::string(buffer, bytes_read);

        //try to parse a single response
        //Returns true even if malformed response.
        HttpResponse response2;
        bool res = response2.parse_response(curr_message);
        if (res) {
            if (response2.parse_error) { //if parse error, just use default 502 bad gateway
                logger.log_error(request_id, "Received invalid response from server.");
                close(sockfd);
                return HttpResponse(); //502 Bad Gateway
            }

            response = response2;
            break; //received full response, done and no need to recv again
        }
    }

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

    size_t pos = server.find(':');
    if (pos != std::string::npos) {
        server = server.substr(0, pos); //get only hostname no port
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve server address
    if (getaddrinfo(server.c_str(), "443", &hints, &res) != 0) {
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        reliable_send(client_socket, error_response.c_str(), error_response.length(), request_id);
        logger.log_error(request_id, "Failed to resolve HTTPS host: " + server);
        return;
    }

    struct addrinfo* it = NULL;
    for (it = res; it != NULL; it = it->ai_next) {
        remote_socket = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (remote_socket == -1) //failed socket creation using current address structure, try next one
            continue;

        if (connect(remote_socket, it->ai_addr, it->ai_addrlen) != -1) //successful connection to ringmaster
            break;

        close(remote_socket); //failed connect using current address structure, try next one
    }

    freeaddrinfo(res);

    if (it == NULL) { //tried all addresses, none succeeded
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        reliable_send(client_socket, error_response.c_str(), error_response.length(), 0);
        logger.log_error(request_id, "Failed to establish HTTPS tunnel to " + server);
        return;
    }


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