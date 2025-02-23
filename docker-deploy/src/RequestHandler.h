#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include "FakeCache.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"

class RequestHandler {
private:
    CacheManager& cache;

public:
    explicit RequestHandler(CacheManager& cache);
    void handle_request(HttpRequest& request, int client_socket);
    HttpResponse forward_request(HttpRequest& request);
    void handle_connect(HttpRequest& request, int client_socket);
};

#endif