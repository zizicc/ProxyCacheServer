#include <sstream>
#include <iostream>

int request_line_parsing_test() {
    //REQUEST LINE PARSING TEST
    std::string line = "GET /test HTTP/1.1\r";
    std::istringstream line_stream(line);

    std::string method, url, http_version;

    //no whitespace is allowed in the three components; this will account for extra whitespace
    std::getline(line_stream, method, ' '); //Read until space
    std::getline(line_stream, url, ' ');
    std::getline(line_stream, http_version); //read remainder of line into http_version
    // line_stream >> method >> url >> http_version >> extra;

    //not enough request line elements or too many, do something about sending HTTP 400
    if (method.empty() || url.empty() || http_version.empty()) {

        // request_str.erase(0, headers_end + sizeof("\r\n\r\n"));
        std::cout << "error 1" << std::endl;
        return 0;
    }

    //http_version should have CR ("\r") at end (required at end of request line). if not, send HTTP 400
    //catches trailing whitespace
    if (http_version[http_version.length()-1] != '\r') {

        // request_str.erase(0, headers_end + sizeof("\r\n\r\n"));
        std::cout << "error 2" << std::endl;
        return 0;
    } else {
        http_version.erase(http_version.length()-1); //remove the \r
    }

    //handle invalid method, url, or http_version
    if (!((method == "GET") || (method == "POST") || (method == "CONNECT"))) {

        std::cout << "error 3" << std::endl;\
        return 0;
    }

    //also catches leading whitespace in http_version and double CR at end
    if (http_version != "HTTP/1.1") {
        std::cout << "error 4" << std::endl;
        return 0;
    }

    std::cout << "no errors" << std::endl;
    std::cout << "method: " << method << " url: " << url << " version: " << http_version << std::endl;

    return 1;
}

int main() {
    request_line_parsing_test();

    return 0;
}