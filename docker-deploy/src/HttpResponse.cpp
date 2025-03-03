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

    if (!std::getline(response_stream, line)) {
        return false;
    }
    
    // Ensure we correctly extract the **entire** HTTP status line
    std::istringstream line_stream(line);
    std::string http_version, status_code, status_message;
    line_stream >> http_version >> status_code;
    std::getline(line_stream, status_message);
    
    // Remove potential leading spaces in status_message
    status_message.erase(0, status_message.find_first_not_of(" "));
    
    // Reconstruct the status line properly
    status_line = http_version + " " + status_code + " " + status_message;
    
    // Trim any trailing CR (if exists)
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }

    // parse headers
    while (std::getline(response_stream, line) && line != "\r") {
        size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            // Trim any trailing newlines or carriage returns from value
            if (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.pop_back();
            }
            headers[key] = value;
        }
    }

    // Parse body properly
    std::ostringstream body_stream;
    bool first_line = true;

    while (std::getline(response_stream, line)) {
        if (!first_line) {
            body_stream << "\n";  // Preserve newlines **except the last one**
        }
        body_stream << line;
        first_line = false;
    }

    // Assign parsed body
    body = body_stream.str();

    // Trim any trailing newlines (to match expected output)
    while (!body.empty() && (body.back() == '\r' || body.back() == '\n')) {
        body.pop_back();
    }

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
    if (status_line.find("200 OK") == std::string::npos) return false;  // Only cache 200 OK

    if (headers.find("Cache-Control") != headers.end()) {
        std::string cache_control = headers.at("Cache-Control");
        if (cache_control.find("no-store") != std::string::npos) return false;  // No caching allowed
        if (cache_control.find("private") != std::string::npos) return false;   // Only for a single user
        if (cache_control.find("must-revalidate") != std::string::npos) requires_validation = true;
    }

    if (status_line.find("206 Partial Content") != std::string::npos) return false; // Don't cache partial responses
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