#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <iostream>
#include <unistd.h>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "RequestHandler.h"
#include "CacheManager.h"
#include "FakeCache.h"

class ClientHandler {
public:
    static void process_client_request(int client_socket, CacheManager& cache);
};

#endif