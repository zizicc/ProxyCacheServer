#include "ClientHandler.h"
#include "HttpRequest.h"
#include "RequestHandler.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <exception>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_set>

ClientHandler::ClientHandler() {

}

ClientHandler::~ClientHandler() {

}

void ClientHandler::handle_client_requests(int client_sockfd, CacheManager& cache) {
    std::unordered_set<std::string> valid_methods = {"GET", "POST"};

    int buffer_read_size = 4096;
    char buffer[buffer_read_size];
    int buffer_index = 0;

    std::string curr_message;

    bool got_headers = false; //true when got all headers for a request
    int request_total_bytes_read; //total bytes read so far for a request

    while (true) {
            int bytes_read = recv(client_sockfd, buffer, buffer_read_size, 0);
            
            if (bytes_read < 0) {
                //means something (i think error)
            } else if (bytes_read == 0) {
                //means something else (i think closed connection)
            }

            curr_message += std::string(buffer, bytes_read); //ensure we copy all buffer, including past any null terminators

            //start parsing our buffer
            //after first recv, 3 cases: 
                //1: did not get a full http request in buffer
                        //maybe didn't get header, or if you did you didn't get full payload 
                //2: got exactly one full http request, including any payload
                //3: got more bytes than a full http request, so could have parts (or entirety) of other requests

            // HTTP-message   = start-line
            // *( header-field CRLF )
            // CRLF --> Carriage Return and Line Feed ("\r\n")
            // [ message-body ]

            while (true) { //could have mutliple http requests in curr_message at once
                HttpRequest request;
                if (!request.parse_request(curr_message)) {
                    
                }

                RequestHandler handler(cache);
                handler.handle_request(request, client_sockfd);

            }

    }
}