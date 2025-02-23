#include "HttpResponse.h"
#include <sstream>
#include <iostream>

HttpResponse::HttpResponse() {
    status_line = "HTTP/1.1 502 Bad Gateway";
    headers["Content-Type"] = "text/html";
    body = "<html><body><h1>502 Bad Gateway</h1></body></html>";
    expiry_time = 0;
    requires_validation = false;
}

HttpResponse::HttpResponse(const std::string& status) {
    status_line = status;
}

bool HttpResponse::parse_response(const std::string& response_str) {
    std::istringstream response_stream(response_str);
    std::string line;

    // parse statue line
    if (!std::getline(response_stream, line)) {
        return false;
    }
    status_line = line;

    // parse headers
    while (std::getline(response_stream, line) && line != "\r") {
        size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            headers[key] = value;
        }
    }

    // parse body
    std::ostringstream body_stream;
    while (std::getline(response_stream, line)) {
        body_stream << line << "\n";
    }
    body = body_stream.str();

    // check if the response is cachable
    if (headers.find("Cache-Control") != headers.end()) {
        std::string cache_control = headers["Cache-Control"];
        if (cache_control.find("no-store") != std::string::npos) {
            return false;  // not cache
        }
        if (cache_control.find("max-age=") != std::string::npos) {
            size_t pos = cache_control.find("max-age=") + 8;
            expiry_time = std::time(nullptr) + std::stoi(cache_control.substr(pos));
        }
    }

    if (headers.find("Expires") != headers.end()) {
        struct tm tm{};
        strptime(headers["Expires"].c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        expiry_time = mktime(&tm);
    }

    return true;
}

bool HttpResponse::is_cacheable() const {
    return expiry_time > std::time(nullptr);
}

std::string HttpResponse::get_header(const std::string& key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? it->second : "";
}

std::string HttpResponse::get_status_line() const {
    return status_line;
}

std::string HttpResponse::get_body() const {
    return body;
}

std::string HttpResponse::serialize() const {
    std::ostringstream response;
    response << status_line << "\r\n";
    for (const auto& header : headers) {
        response << header.first << ": " << header.second << "\r\n";
    }
    response << "\r\n" << body;
    return response.str();
}