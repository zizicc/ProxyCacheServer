#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <vector>
#include <list>
#include "ClientHandler.h"
#include "FakeCache.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

// #define PROXY_SERVER_PORT 80

class ProxyServer {
private:

public:
    int proxy_server_port;
    int listening_sockfd;

    // std::vector<ClientHandler> clients; 
    // std::mutex clients_lock;
    std::list<std::thread> client_threads; //changed to list to avoid iterator invalidation
    std::mutex client_threads_lock;
    std::atomic<bool> stop_flag; //flag to signal threads to shutdown

    std::queue<std::list<std::thread>::iterator> reaper_q; //queue for finished threads, reaper thread will join them
    std::mutex reaper_q_lock;
    std::condition_variable reaper_q_cv;
    std::thread reaper_thread;

    CacheManager cache;

    ProxyServer(int proxy_server_port);
    ~ProxyServer();

    void handle_client(int client_sockfd, std::list<std::thread>::iterator it);
    void cleanup_threads();
    void start();
};

#endif