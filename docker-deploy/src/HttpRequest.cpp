#include "HttpRequest.h"
#include <sstream>
#include <iostream>
#include <unordered_set>

//ensure field name is valid token
//also catches spaces between field-name and colon
bool HttpRequest::valid_field_name(const std::string& field) {
    if (field.empty()) return false;

    const std::string validSpecialChars = "!#$%&'*+-.^_`|~"; //allowed special chars besides a-z,0-9 in token

    for (char c : field) {
        if (!(std::isalnum(c) || validSpecialChars.find(c) != std::string::npos)) {
            return false; //invalid character found
        }
    }

    return true;
}

//removes any leading or trailing OWS (space, tab) from field-value
//result is an empty string if entire field-value is OWS
void HttpRequest::trim_field_value(std::string& value) {
    const std::string whitespace = " \t";
    size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) { //value is all spaces/tabs
        value = "";
        return;
    } 

    size_t end = value.find_last_not_of(whitespace);
    value = value.substr(start, end - start + 1);

}

//Based on RFC:
        // A sender MUST NOT generate multiple header fields with the same field
        // name in a message unless either the entire field value for that
        // header field is defined as a comma-separated list [i.e., #(values)]
        // or the header field is a well-known exception (as noted below).
//This function checks if a field name we found to be duplicated across multiple headers is allowed
bool HttpRequest::can_duplicate_field_name(std::string& field_name) {
    static const std::unordered_set<std::string> can_duplicate = { //found list here: https://stackoverflow.com/questions/52272217/which-http-headers-can-be-combined-in-a-list
        "A-IM",
        "Accept",
        "Accept-Charset",
        "Accept-Encoding",
        "Accept-Language",
        "Access-Control-Request-Headers",
        "Cache-Control",
        "Connection",
        "Content-Encoding",
        "Expect",
        "Forwarded",
        "If-Match",
        "If-None-Match",
        "Range",
        "TE",
        "Trailer",
        "Transfer-Encoding",
        "Upgrade",
        "Via",
        "Warning",
        "Set-Cookie" //special case, cant actually combine, only is responses though
    };

    return can_duplicate.find(field_name) != can_duplicate.end();
}

//only returns false when we weren't able to parse request yet because not enough data (e.g. no end of header, some of body missing). 
//returns true if we have something valid to pass to RequestHandler, whether
//that be a request that needs to be responded to with 400 error or a valid request 
bool HttpRequest::parse_request(std::string& request_str) {
    //a processable http request will at least have end of header ("\r\n\r\n", aka double CRLF)
    //otherwise, incomplete http request (can't process yet, must recv more data) or junk data
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
        client_error_code = 400;
        //after return, requestHandler should close connection with client after sending error code
        //no need to clean up rest of buffer, all of it should be invalidated
        std::cout << "error 1" << std::endl;
        // request_str.erase(0, headers_end + sizeof("\r\n\r\n")); //also erase invalid data up to headers_end
        return true;
    }

    request_str.erase(0, method_start); //make start of request line start of string
    headers_end = request_str.find("\r\n\r\n"); //reobtain index of end of header
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

    //no whitespace is allowed in the three components; this will account for extra whitespace
    std::getline(line_stream, method, ' '); //Read until space
    std::getline(line_stream, url, ' ');
    std::getline(line_stream, http_version); //read remainder of line into http_version
    // line_stream >> method >> url >> http_version >> extra;

    //not enough request line elements or too many, do something about sending HTTP 400
    if (method.empty() || url.empty() || http_version.empty()) {
        client_error_code = 400;
        std::cout << "error 2" << std::endl;

        return true;
    }

    //http_version should have CR ("\r") at end (required at end of request line). if not, send HTTP 400
    //catches trailing whitespace
    if (http_version[http_version.length()-1] != '\r') {
        client_error_code = 400;
        std::cout << "error 3" << std::endl;

        return true;
    } else {
        http_version.erase(http_version.length()-1); //remove the \r
    }

    //handle invalid method, url, or http_version
    if (!((method == "GET") || (method == "POST") || (method == "CONNECT"))) {
        client_error_code = 400;
        std::cout << "error 4" << std::endl;
        return true;
    }

    //also catches leading whitespace in http_version
    if (http_version != "HTTP/1.1") {
        client_error_code = 400;
        std::cout << "error 5" << std::endl;
        return true;
    }

    //dont really know how to validate url?


    bool has_chunked = false; //used for later

    //PARSE HEADERS
    //header-field   = field-name ":" OWS field-value OWS
    // field-name     = token
    // field-value    = *( field-content / obs-fold )
    while (std::getline(request_stream, line)) {
        chars_read += line.length() + 1;

        size_t pos = line.find(":");
        if (pos == std::string::npos) { //no colon in a header field, bad. 400 response
            client_error_code = 400;
            std::cout << "error 6" << std::endl;
            return true;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1); //remainder after colon

            //A server MUST reject any received request message that contains
            //whitespace between a header field-name and colon with a response code of 400
            //also ensure field-name is valid token
        if (!HttpRequest::valid_field_name(key)) {
            client_error_code = 400;
            std::cout << "error 7" << std::endl;
            return true;
        }

        if (value[value.length() - 1] != '\r') { //ensure field-value ends with CRLF
            client_error_code = 400;
            std::cout << "error 8" << std::endl;
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
                client_error_code = 400;
                std::cout << "error 9" << std::endl;
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
        std::cout << "somehow don't have redundant CRLF at end of headers when we expected to" << std::endl;
        client_error_code = 400;
        std::cout << "error 10" << std::endl;
        return true;
    }

    //BY HERE, HEADERS HAVE BEEN PARSED PROPERLY

    //a host header field must be sent in all HTTP/1.1 request messages
    if (headers.find("Host") != headers.end()) {
        host = headers["Host"];
    } else { 
        client_error_code = 400; //no host, return 400
        std::cout << "error 11" << std::endl;
        return true;
    }

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
            if (bodyStart + len > request_str.length()) {
                //not enough data yet to read full body, need more recv;
                return false;
            }
            body = request_str.substr(bodyStart, len); //set body field of HttpRequest
            chars_read += len;
            
        } catch (...) { //catch invalid content-length field values
            client_error_code = 400;
            std::cout << "error 13" << std::endl;
            return true;
        }


    } else if (headers.find("Transfer-Encoding") != headers.end()) { //chunked transfer encoding
        //According to RFC:
        //    If a Transfer-Encoding header field
        //    is present in a request and the chunked transfer coding is not
        //    the final encoding, the message body length cannot be determined
        //    reliably; the server MUST respond with the 400 (Bad Request)
        //    status code and then close the connection.
        if (!has_chunked) { //has transfer encoding but chunked isnt one of them
            client_error_code = 400;
            std::cout << "error 14" << std::endl;
            return true;
        }

        uint32_t len = 0;
        int bodyStart = chars_read;
        int curr_ptr = bodyStart;

        while (true) {
            size_t line_end;
            if ((line_end = request_str.find("\r\n", curr_ptr)) == std::string::npos) {
                return false; //havent received the end of a chunked body line, more recv
            }

            chars_read += line_end - curr_ptr + 2;
            line = request_str.substr(curr_ptr, line_end - curr_ptr);
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

            if (curr_ptr + chunk_size + 2 > request_str.length()) {
                return false; //havent received end of chunk data line, more recv
            }

            if (request_str.substr(curr_ptr + chunk_size, 2) != "\r\n") { //ill format
                client_error_code = 400;
                std::cout << "error 15" << std::endl;
                return true;
            }

            body += request_str.substr(curr_ptr, chunk_size); //append chunk data to decoded body
            chars_read += chunk_size + 2;
            curr_ptr = chars_read;
            len += chunk_size;
        }

        //read optional trailer part
        while (true) {
            size_t line_end;
            if ((line_end = request_str.find("\r\n", curr_ptr)) == std::string::npos) {
                std::cout << "havent received either a full trailer-part line or crlf" << std::endl; 
                return false; //havent received either a trailer-part or crlf, more recv
            }

            chars_read += line_end - curr_ptr + 2;
            line = request_str.substr(curr_ptr, line_end - curr_ptr);
            curr_ptr = chars_read;

            if (line.empty()) { //final CRLF of chunked body, done now
                break;
            }

            size_t pos = line.find(":");
            if (pos == std::string::npos) { //no colon in a header field, bad. 400 response
                client_error_code = 400;
                std::cout << "error 16" << std::endl;
                return true;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1); //remainder after colon

                //A server MUST reject any received request message that contains
                //whitespace between a header field-name and colon with a response code of 400
                //also ensure field-name is valid token
            if (!HttpRequest::valid_field_name(key)) {
                client_error_code = 400;
                std::cout << "error 17" << std::endl;
                return true;
            }

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
                client_error_code = 400;
                std::cout << "error 18" << std::endl;
                return true;
            }

            if (headers.find(key) != headers.end()) { //found duplicate header field names
                if (!HttpRequest::can_duplicate_field_name(key)) { //error, cant have multiple of this header field name
                    client_error_code = 400;
                    std::cout << "error 19" << std::endl;
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

    //if we've gotten here we have a valid http request, remove from string request_str
    std::cout << "no errors parsing" << std::endl;
    request_str.erase(0, chars_read); //REMOVE CURRENT HTTP REQUEST FROM REQUEST_STR
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

std::string HttpRequest::get_http_version() const {
    return http_version;
}

bool HttpRequest::has_header(const std::string& key) const {
    return headers.find(key) != headers.end();
}

void HttpRequest::add_header(const std::string& key, const std::string& value) {
    headers[key] = value;
}