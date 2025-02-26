#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "CacheManager.h"
#include <atomic>

class ClientHandler {
private:

public:
    ClientHandler();
    ~ClientHandler();

    void handle_client_requests(int client_sockfd, CacheManager& cache, std::atomic<bool>& stop_flag);
};


#endif