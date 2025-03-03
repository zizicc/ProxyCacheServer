#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "CacheManager.h"

class RequestHandler {
private:
    CacheManager& cache;

    HttpResponse forward_request(HttpRequest& request, int request_id);
    void handle_connect(HttpRequest& request, int client_socket, int request_id);

public:
    explicit RequestHandler(CacheManager& cache);
    int handle_request(HttpRequest& request, int client_socket, int request_id, const std::string& client_ip);
};

#endif