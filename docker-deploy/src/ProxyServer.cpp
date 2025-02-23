#include "ProxyServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <exception>
#include <cstring>

//if object construction fails (cant create socket or bind it), throw a runtime exception
ProxyServer::ProxyServer(int proxy_server_port) : proxy_server_port(proxy_server_port), stop_flag(false) { //im guessing cache has some default initialization that doesn't require args
    listening_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_sockfd < 0) {
        throw std::runtime_error("Failed to create Proxy's listener socket");
    }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; //binds socket to all available interfaces (ip addresses), not just localhost. this allows it to accept connections
    address.sin_port = htons(this->proxy_server_port);

    if (bind(listening_sockfd, (sockaddr*)&address, sizeof(address)) < 0) { //bind socket to port PROXY_SERVER_PORT
        close(listening_sockfd); //needed since ~ProxyServer() won't be called since object isn't considered fully constructed
        throw std::runtime_error("Failed to bind socket to port");
    }
}


ProxyServer::~ProxyServer() { //might need to declare noexcept, idk?
    close(listening_sockfd); //close listening socket; all client sockets should get closed by respective thread

    //ensure all threads in client_threads are joined before they go out of scope (vector client_threads destructing = threads destructing b/c out of scope)
    stop_flag = true;

    //wakeup reaper thread--guaranteed to see stop flag once done
    {
        std::lock_guard<std::mutex> guard(reaper_q_lock);
        reaper_q_cv.notify_one();
    } //lock released

    reaper_thread.join(); //wait for reaper thread to finish

    {
        std::lock_guard<std::mutex> guard(client_threads_lock); //dont need but i dont think it hurts

        //cleanup anyone reaper_thread did not (anyone who didn't put self in reaper queue before it did last cleanup)
        for (auto& a : client_threads) {
            if (a.joinable()) { //includes threads that have finished execution and those that have not
                a.join(); //blocking, wait for thread to finish if not or immediately join if already finished
            }
        }
    } //lock released

    //doesn't matter if reaper_q has some iterators in it, all threads referenced by iterators are joined

    std::cout << "All threads joined. Proxy server shutting down..." << std::endl;

}

//iterator needed so when thread finishes it can add itself to reaper queue
void ProxyServer::handle_client(int client_sockfd, std::vector<std::thread>::iterator it) {
    //enter here with a worker thread

    ClientHandler handler; //thread creates client handler
    handler.handle_client_requests(client_sockfd); //returns when thread finishes (connection closed or server shutdown)

    close(client_sockfd);

    //when a thread finishes, put itself in reaper queue
    std::lock_guard<std::mutex> guard(reaper_q_lock);

    reaper_q.push(it);

    //signal reaper thread to wake up
    reaper_q_cv.notify_one();

    //lock guard destructed, lock released
    //thread now finished executing
}

//reaper thread cleans up finished threads (in reaper queue) when woken up by cv signal
void ProxyServer::cleanup_threads() {
    
    std::unique_lock<std::mutex> lk(reaper_q_lock, std::defer_lock); //basically same as lock guard except can actually be locked and unlocked while
                                                                     //lock guard can only be unlocked on destruction (unlock needed for cv.wait())
    while (!stop_flag) {
        lk.lock();
        reaper_q_cv.wait(lk, [&]{ return ((reaper_q.size() > 0) || (stop_flag)); }); //ensures we wait again unless elements to clean or stop flag set

        //we own lock, can clean up
        while (!reaper_q.empty()) {
            std::vector<std::thread>::iterator it = reaper_q.front();
            reaper_q.pop();

            if (it->joinable()) { //join the thread
                it->join();
            }

            //can now remove thread from client_threads (destructs it) since join called
            std::lock_guard<std::mutex> guard(client_threads_lock);
            client_threads.erase(it);
        } //client threads lock guard released

        //done, so unlock. no need to wake up anyone else since we only one waiting
        lk.unlock();
    }

    //thread now finished executing
}

//let the proxy server start listening and accepting connections. This is a blocking function.
void ProxyServer::start() {
    if (listen(listening_sockfd, 128) < 0) {
        throw std::runtime_error("Failed to set up socket to listen for incoming connections");
    }

    std::cout << "Proxy server listening on port " << proxy_server_port << std::endl;

    sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address));
    socklen_t client_address_len = sizeof(client_address);

    //initialize reaper/cleanup thread
    reaper_thread = std::thread(&ProxyServer::cleanup_threads, this);
    
    //start acccepting connections
    while (true) { //accept connection requests here; creates new socket and bounds that socket to same port as listener socket
        int client_connection_sockfd = accept(listening_sockfd, (sockaddr*)&client_address, &client_address_len);

        if (client_connection_sockfd < 0) {
            std::cout << "Could not accept new client connection request" << std::endl;
            continue; //don't exit here, try to accept again
        }

        //create new thread to call handle_client()
        std::lock_guard<std::mutex> guard(client_threads_lock); //lock guard to ensure client_threads_lock is released either when guard goes out of scope or exception in creating new thread
        client_threads.emplace_back(); //add a default-constructed thread
        auto it = --client_threads.end(); //get iterator after insertion
        *it = std::thread([this, client_connection_sockfd, it]() { handle_client(client_connection_sockfd, it); });
        //client_threads.emplace_back(&ProxyServer::handle_client, this, client_connection_sockfd, --client_threads.end()); //add thread to list of all active threads
    }
    
}