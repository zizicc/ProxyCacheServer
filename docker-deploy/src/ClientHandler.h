#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "CacheManager.h"
#include <atomic>

class ClientHandler {
private:

public:
    ClientHandler();
    ~ClientHandler();

    void handle_client_requests(int client_sockfd, CacheManager& cache, std::atomic<bool>& stop_flag, std::atomic_int& curr_request_id, std::string& client_ip);
};


#endif