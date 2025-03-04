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
    int client_error_code; //if not 0, indicates client error in request (4xx)

    HttpRequest() = default;
    bool parse_request(std::string& request_str);
    
    std::string get_method() const;
    std::string get_url() const;
    std::string get_host() const;
    std::string get_header(const std::string& key) const;
    std::string get_body() const;
    std::string serialize() const;
    std::string get_http_version() const;
    bool has_header(const std::string& key) const;
    void add_header(const std::string& key, const std::string& value);

    static bool valid_field_name(const std::string& field);
    static void trim_field_value(std::string& value);
    static bool can_duplicate_field_name(std::string& field_name);

};

#endif