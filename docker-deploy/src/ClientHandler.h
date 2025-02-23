#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

class ClientHandler {
private:

public:
    ClientHandler();
    ~ClientHandler();

    void handle_client_requests(int client_sockfd);
};


#endif