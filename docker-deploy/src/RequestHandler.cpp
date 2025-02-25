#include "RequestHandler.h"
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

RequestHandler::RequestHandler(CacheManager& cache) : cache(cache) {}

void RequestHandler::handle_request(HttpRequest& request, int client_socket) {
    std::string method = request.get_method();
    std::string url = request.get_url();

    // handle GET requst
    if (method == "GET") {
        if (cache.is_in_cache(url)) {
            std::shared_ptr<HttpResponse> cached_response = cache.get_cached_response(url);
            if (cached_response) {
                std::string response_str = cached_response->serialize();
                send(client_socket, response_str.c_str(), response_str.length(), 0);
                Logger().log_cache_status(0, "in cache, valid");
                Logger().log_response(0, cached_response->get_status_line());
                return;
            }
        }
        Logger().log_cache_status(0, "not in cache");
    }

    // handle CONNECT  (HTTPS tunnel)
    if (method == "CONNECT") {
        handle_connect(request, client_socket);
        return;
    }

    // other requests（including POST）transfer directly
    HttpResponse response = forward_request(request);
    std::string response_str = response.serialize();
    send(client_socket, response_str.c_str(), response_str.length(), 0);

    // only 200-OK for GET response is cached
    if (method == "GET" && response.is_cacheable()) {
        cache.store_response(url, std::make_shared<HttpResponse>(response));
        Logger().log_cache_status(0, "cached, expires at " + response.get_header("Expires"));
    }

    Logger().log_response(0, response.get_status_line());
}

HttpResponse RequestHandler::forward_request(HttpRequest& request) {
    int sockfd;
    struct addrinfo hints{}, *res;

    // get the server address
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(request.get_host().c_str(), "80", &hints, &res) != 0) {
        return HttpResponse(); // 502 Bad Gateway
    }

    // creat socket and connect
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        return HttpResponse(); // 502 Bad Gateway
    }

    freeaddrinfo(res);

    // send requests
    std::string request_str = request.serialize();
    send(sockfd, request_str.c_str(), request_str.length(), 0);

    // read responsed from server
    char buffer[8192];
    int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        close(sockfd);
        return HttpResponse(); // 502 Bad Gateway
    }

    // parse http request
    HttpResponse response;
    response.parse_response(std::string(buffer, bytes_received));

    close(sockfd);
    return response;
}

void RequestHandler::handle_connect(HttpRequest& request, int client_socket) {
    int remote_socket;
    struct addrinfo hints{}, *res;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(request.get_host().c_str(), "443", &hints, &res) != 0) {
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_socket, error_response.c_str(), error_response.length(), 0);
        return;
    }

    remote_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(remote_socket, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_socket, error_response.c_str(), error_response.length(), 0);
        return;
    }

    freeaddrinfo(res);

    // 200 OK to client, tunnel
    std::string success_response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_socket, success_response.c_str(), success_response.length(), 0);

    // pass through
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
    Logger().log_cache_status(0, "Tunnel closed");
}