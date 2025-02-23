#include "HttpRequest.h"
#include <sstream>
#include <iostream>

bool HttpRequest::parse_request(const std::string& request_str) {
    std::istringstream request_stream(request_str);
    std::string line;

    // get the status
    if (!std::getline(request_stream, line)) {
        return false;
    }

    std::istringstream line_stream(line);
    line_stream >> method >> url >> http_version;

    if (method.empty() || url.empty() || http_version.empty()) {
        return false;
    }

    // parse header
    while (std::getline(request_stream, line) && line != "\r") {
        size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            headers[key] = value;
        }
    }

    // get the host header
    if (headers.find("Host") != headers.end()) {
        host = headers["Host"];
    } else {
        return false;
    }

    // parse body if there is one
    std::ostringstream body_stream;
    while (std::getline(request_stream, line)) {
        body_stream << line << "\n";
    }
    body = body_stream.str();

    return true;
}

std::string HttpRequest::get_method() const {
    return method;
}

std::string HttpRequest::get_url() const {
    return url;
}

std::string HttpRequest::get_host() const {
    return host;
}

std::string HttpRequest::get_header(const std::string& key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? it->second : "";
}

std::string HttpRequest::get_body() const {
    return body;
}

std::string HttpRequest::serialize() const {
    std::ostringstream request;
    request << method << " " << url << " " << http_version << "\r\n";

    for (const auto& header : headers) {
        request << header.first << ": " << header.second << "\r\n";
    }

    request << "\r\n" << body;
    return request.str();
}