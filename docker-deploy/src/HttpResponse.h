#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <string>
#include <unordered_map>
#include <ctime>

class HttpResponse {
private:


public:
    std::string status_line;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    time_t expiry_time = 86400;
    mutable bool requires_validation;
    
    HttpResponse();
    explicit HttpResponse(const std::string& status);
    bool parse_response(std::string& response_str);
    bool is_cacheable() const;
    std::string get_header(const std::string& key) const;
    std::string get_status_line() const;
    std::string get_body() const;
    std::string serialize() const;
    void print_headers();

    bool parse_error;
};

#endif