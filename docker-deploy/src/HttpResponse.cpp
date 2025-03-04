#include "HttpResponse.h"
#include <sstream>
#include <iostream>
#include "HttpRequest.h"


HttpResponse::HttpResponse() : parse_error(false) {
    status_line = "HTTP/1.1 502 Bad Gateway";
    headers["Content-Type"] = "text/html";
    body = "<html><body><h1>502 Bad Gateway</h1></body></html>";
    expiry_time = 0;
    requires_validation = false;
}

HttpResponse::HttpResponse(const std::string& status) {
    status_line = status;
}

//Returns true even if malformed response.
//only returns false when we weren't able to parse response yet because not enough data
bool HttpResponse::parse_response(std::string& response_str) {
    status_line = "";
    headers.clear();
    body = "";
    //a processable http response will at least have end of header ("\r\n\r\n", aka double CRLF)
    //otherwise, incomplete http response (can't process yet, must recv more data) or junk data
    size_t headers_end;
    if ((headers_end = response_str.find("\r\n\r\n")) == std::string::npos) {
        return false;
    }

    //valid header end, so get status line
    size_t status_start = response_str.find("HTTP");

    if (status_start == std::string::npos) { //not valid header
        std::cout << "error 1" << std::endl;
        parse_error = true;
        return true;
    }

    response_str.erase(0, status_start); //make start of response line start of string
    headers_end = response_str.find("\r\n\r\n"); //reobtain index of end of header
    int chars_read = 0; //how many chars we have read

    std::istringstream request_stream(response_str);
    std::string line;

        // HTTP-message   = status-line = HTTP-version SP status-code SP reason-phrase CRLF
        // *( header-field CRLF )
        // CRLF --> Carriage Return and Line Feed ("\r\n")
        // [ message-body ]

    //PARSE STATUS LINE
    if (!std::getline(request_stream, line)) {
        //not sure what to do here, error?
    }

    chars_read += line.length() + 1; //account for "\n" that getline extracted from stream but didn't put into line

    std::istringstream line_stream(line);
    std::string http_version, status_code, status_message;
    line_stream >> http_version >> status_code;
    std::getline(line_stream, status_message);
    
    //Remove potential leading spaces in status_message
    status_message.erase(0, status_message.find_first_not_of(" "));
    
    //Reconstruct the status line properly
    status_line = http_version + " " + status_code + " " + status_message;
    
    //Trim any trailing CR (if exists)
    if (!status_line.empty() && status_line[status_line.length() - 1] == '\r') {
        status_line.pop_back();
    }

    bool has_chunked = false; //used for later

    //PARSE HEADERS
    //header-field   = field-name ":" OWS field-value OWS
    // field-name     = token
    // field-value    = *( field-content / obs-fold )
    while (std::getline(request_stream, line)) {
        chars_read += line.length() + 1;

        size_t pos = line.find(":");
        if (pos == std::string::npos) { //no colon in a header field, bad.
            std::cout << "error 2" << std::endl;
            parse_error = true;
            return true;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1); //remainder after colon


        if (value[value.length() - 1] != '\r') { //ensure field-value ends with CRLF
            std::cout << "error 3" << std::endl;
            parse_error = true;
            return true;
        } else {
            value.erase(value.length()-1); //remove the CR
        }

        //parse field-value to remove any leading or trailing OWS (optional whitespace)
        //if all of field-value is OWS, field-value will just be an empty string indicating no field value 
        HttpRequest::trim_field_value(value);
        if (key == "Transfer-Encoding" && value == "chunked") {
            has_chunked = true;
        }

        if (headers.find(key) != headers.end()) { //found duplicate header field names
            if (!HttpRequest::can_duplicate_field_name(key)) { //error, cant have multiple of this header field name
                std::cout << "error 4" << std::endl;
                parse_error = true;
                return true;
            }

            if (key != "Set-Cookie" && !value.empty()) { //cant combine set-cookie but exception, just move on and use 1st val
                headers[key] += ", " + value;
            }

        } else {
            headers[key] = value;
        }

        if (chars_read == headers_end + sizeof("\r\n") - 1) { //should be final header before double CRLF
            break;
        }
    }

    //ensure we have another CRLF to indicate end of headers
    //shouldn't ever not happen but adding for safety
    std::getline(request_stream, line);
    chars_read += line.length() + 1;
    if (line != "\r") {
        std::cout << "error 5" << std::endl;
        parse_error = true;
        return true;
    }

    //BY HERE, HEADERS HAVE BEEN PARSED PROPERLY
    // parse body if there is one

    //If a message is received with both a Transfer-Encoding and a
    // Content-Length header field, the Transfer-Encoding overrides the
    // Content-Length.
    if (headers.find("Content-Length") != headers.end() && headers.find("Transfer-Encoding") != headers.end()) { //bad to have both
        headers.erase("Content-Length");
    }

    
    if (headers.find("Content-Length") != headers.end()) { //content length
        try {
            uint32_t len = stoul(headers["Content-Length"]);
            int bodyStart = chars_read;
            if (bodyStart + len > response_str.length()) {
                //not enough data yet to read full body, need more recv;
                return false;
            }
            body = response_str.substr(bodyStart, len); //set body field of HttpRequest
            chars_read += len;
            
        } catch (...) { //catch invalid content-length field values
            std::cout << "error 6" << std::endl;
            parse_error = true;
            return true;
        }


    } else if (headers.find("Transfer-Encoding") != headers.end()) { //chunked transfer encoding

        if (!has_chunked) { //has transfer encoding but chunked isnt one of them
            std::cout << "error 7" << std::endl;
            parse_error = true;
            return true;
        }

        uint32_t len = 0;
        int bodyStart = chars_read;
        int curr_ptr = bodyStart;

        while (true) {
            size_t line_end;
            if ((line_end = response_str.find("\r\n", curr_ptr)) == std::string::npos) {
                return false; //havent received the end of a chunked body line, more recv
            }

            chars_read += line_end - curr_ptr + 2;
            line = response_str.substr(curr_ptr, line_end - curr_ptr);
            curr_ptr = chars_read;

            std::string chunk_size_str;
            if (line.find(';') != std::string::npos) { //there are chunk extensions
                chunk_size_str = line.substr(0, line.find(';'));
            } else {
                chunk_size_str = line;
            }

            uint32_t chunk_size = std::stoul(chunk_size_str, nullptr, 16);

            if (chunk_size <= 0) { //this is "last-chunk", end of chunks
                break;
            }

            if (curr_ptr + chunk_size + 2 > response_str.length()) {
                return false; //havent received end of chunk data line, more recv
            }

            if (response_str.substr(curr_ptr + chunk_size, 2) != "\r\n") { //ill format
                std::cout << "error 8" << std::endl;
                parse_error = true;
                return true;
            }

            body += response_str.substr(curr_ptr, chunk_size); //append chunk data to decoded body
            chars_read += chunk_size + 2;
            curr_ptr = chars_read;
            len += chunk_size;
        }

        //read optional trailer part
        while (true) {
            size_t line_end;
            if ((line_end = response_str.find("\r\n", curr_ptr)) == std::string::npos) {
                std::cout << "havent received either a full trailer-part line or crlf" << std::endl; 
                return false; //havent received either a trailer-part or crlf, more recv
            }

            chars_read += line_end - curr_ptr + 2;
            line = response_str.substr(curr_ptr, line_end - curr_ptr);
            curr_ptr = chars_read;

            if (line.empty()) { //final CRLF of chunked body, done now
                break;
            }

            size_t pos = line.find(":");
            if (pos == std::string::npos) { //no colon in a header field, bad. 400 response
                std::cout << "error 9" << std::endl;
                parse_error = true;
                return true;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1); //remainder after colon

            //parse field-value to remove any leading or trailing OWS (optional whitespace)
            //if all of field-value is OWS, field-value will just be an empty string indicating no field value 
            HttpRequest::trim_field_value(value);

            // A sender MUST NOT generate a trailer that contains a field necessary
            // for message framing (e.g., Transfer-Encoding and Content-Length),
            // routing (e.g., Host), request modifiers (e.g., controls and
            // conditionals in Section 5 of [RFC7231]), authentication (e.g., see
            // [RFC7235] and [RFC6265]), response control data (e.g., see Section
            // 7.1 of [RFC7231]), or determining how to process the payload (e.g.,
            // Content-Encoding, Content-Type, Content-Range, and Trailer).
            if (key == "Transfer-Encoding" || key == "Content-Length" || key == "Host" || key == "Content-Encoding" || 
                key == "Content-Type" || key == "Content-Range" || key == "Trailer") {
                std::cout << "error 10" << std::endl;
                parse_error = true;
                return true;
            }

            if (headers.find(key) != headers.end()) { //found duplicate header field names
                if (!HttpRequest::can_duplicate_field_name(key)) { //error, cant have multiple of this header field name
                    std::cout << "error 11" << std::endl;
                    parse_error = true;
                    return true;
                }

                if (key != "Set-Cookie" && !value.empty()) { //cant combine set-cookie but exception, just move on and use 1st val
                    headers[key] += ", " + value;
                }

            } else {
                headers[key] = value;
            }
        }

        //Content-Length := length
        headers["Content-Length"] = len;

        //Remove "chunked" from Transfer-Encoding
        headers.erase("Transfer-Encoding");

        //Remove Trailer from existing header fields
        headers.erase("Trailer");

    } else { //message body length = 0 since none of above 2 headers

    }


    // //check if the response is cachable
    // if (headers.find("Cache-Control") != headers.end()) {
    //     std::string cache_control = headers["Cache-Control"];
    //     if (cache_control.find("no-store") != std::string::npos) {
    //         return false;  // not cache
    //     }
    //     if (cache_control.find("max-age=") != std::string::npos) {
    //         size_t pos = cache_control.find("max-age=") + 8;
    //         expiry_time = std::time(nullptr) + std::stoi(cache_control.substr(pos));
    //     }
    // }

    // if (headers.find("Expires") != headers.end()) {
    //     struct tm tm{};
    //     strptime(headers["Expires"].c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    //     expiry_time = mktime(&tm);
    // }


    //if we've gotten here we have a valid http response
    std::cout << "response: no errors parsing" << std::endl;
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