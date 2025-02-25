#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <string>
#include <unordered_map>

class HttpRequest {
private:
    std::string method;
    std::string url;
    std::string host;
    std::string http_version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

public:
    HttpRequest() = default;
    bool parse_request(std::string& request_str);
    std::string get_method() const;
    std::string get_url() const;
    std::string get_host() const;
    std::string get_header(const std::string& key) const;
    std::string get_body() const;
    std::string serialize() const;
};

#endif