#include "HttpRequest.h"
#include <sstream>
#include <iostream>

bool HttpRequest::parse_request(std::string& request_str) {
    //a processable http request will at least have end of header ("\r\n\r\n")
    //otherwise, incomplete http request (can't process yet, recv more data) or junk data
    size_t headers_end;
    if ((headers_end = request_str.find("\r\n\r\n")) == std::string::npos) {
        return false;
    }

    //valid header end, so get request line
    size_t get_start = request_str.find("GET");
    size_t post_start = request_str.find("POST");
    size_t connect_start = request_str.find("CONNECT");

    //find earliest occurance of method since might be multiple
    size_t method_start = std::min(std::min(get_start, post_start), connect_start);

    if (method_start == std::string::npos) { //no valid method, do something about sending HTTP 400

        request_str.erase(0, headers_end + sizeof("\r\n\r\n")); //also erase invalid data up to headers_end
        return false;
    }

    request_str.erase(0, method_start); //make start of request line start of string
    int chars_read = 0; //how many chars we have read

    std::istringstream request_stream(request_str);
    std::string line;

        // HTTP-message   = request-line = method SP request-target SP HTTP-version CRLF
        // *( header-field CRLF )
        // CRLF --> Carriage Return and Line Feed ("\r\n")
        // [ message-body ]

    //PARSE REQUEST LINE
    if (!std::getline(request_stream, line)) {
        //not sure what to do here, error?
    }
    chars_read += line.length() + 1; //account for "\n" that getline extracted from stream but didn't put into line

    std::istringstream line_stream(line);

    std::string extra;
    line_stream >> method >> url >> http_version >> extra;

    //not enough request line elements or too many, do something about sending HTTP 400
    if (method.empty() || url.empty() || http_version.empty() || !(extra.empty()) ) {
        return false;
    }

    //http_version should have CR ("\r") at end (required at end of request line). if not, send HTTP 400
    if (http_version[http_version.length()-1] != '\r') {
        return false;
    } else {
        http_version.erase(http_version.length()-1); //remove the \r
    }

    //handle invalid method, url, or http_version



    //PARSE HEADER
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