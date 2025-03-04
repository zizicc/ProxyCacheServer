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

ClientHandler::ClientHandler() {

}

ClientHandler::~ClientHandler() {

}

void ClientHandler::handle_client_requests(int client_sockfd, CacheManager& cache, std::atomic<bool>& stop_flag, std::atomic_int& curr_request_id, std::string& client_ip) {

    int buffer_read_size = 4096;
    char buffer[buffer_read_size];
    int buffer_index = 0;

    std::string curr_message;

    bool got_headers = false; //true when got all headers for a request
    int request_total_bytes_read; //total bytes read so far for a request

    bool force_connection_close = false;

    while (!stop_flag && !force_connection_close) { //will stop when it sees stop flag is set to shutdown socket and thread (or forced close)
            int bytes_read = recv(client_sockfd, buffer, buffer_read_size, 0);
            
            if (bytes_read < 0) {
                //means something (i think error)
            } else if (bytes_read == 0) {
                //client closed connection
                break;
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

                //if string curr_message doesn't contain full http message yet, returns false.
                //Returns true otherwise, even if malformed message. 
                //if gets valid http message, parses the message fully and removes from curr_message
                bool res = request.parse_request(curr_message);
                if (!res) {
                    break; //need to recv again
                }

                int request_id = curr_request_id++;
                //if request.parse_request determined malformed request (error code 4xx)
                //then request.client_error_code will be set and handler should send
                //error response to client AND THEN WE SHOULD CLOSE CONNECTION (return from handle_client_requests)                       
                RequestHandler handler(cache);
                int cont = handler.handle_request(request, client_sockfd, curr_request_id, client_ip);
                if (cont == -1) { //close connection now
                    force_connection_close = true;
                    break; //dont care about handling anything else from buffer
                }

            }

    }
}