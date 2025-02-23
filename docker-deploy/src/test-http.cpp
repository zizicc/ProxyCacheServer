#include <iostream>
#include "RequestHandler.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "FakeCache.h"

void test_http_request() {
    std::string raw_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "\r\n";

    HttpRequest request;
    if (!request.parse_request(raw_request)) {
        std::cerr << "Failed to parse HTTP request\n";
        return;
    }

    std::cout << "Parsed Method: " << request.get_method() << std::endl;
    std::cout << "Parsed URL: " << request.get_url() << std::endl;
    std::cout << "Parsed Host: " << request.get_host() << std::endl;
    std::cout << "Serialized Request:\n" << request.serialize() << std::endl;
}

void test_http_response() {
    std::string raw_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Cache-Control: max-age=3600\r\n"
        "\r\n"
        "Hello, World!";

    HttpResponse response;
    if (!response.parse_response(raw_response)) {
        std::cerr << "Failed to parse HTTP response\n";
        return;
    }

    std::cout << "Parsed Status Line: " << response.get_status_line() << std::endl;
    std::cout << "Parsed Content-Type: " << response.get_header("Content-Type") << std::endl;
    std::cout << "Is Cacheable: " << (response.is_cacheable() ? "Yes" : "No") << std::endl;
    std::cout << "Serialized Response:\n" << response.serialize() << std::endl;
}

void test_request_handler() {
    CacheManager cache;
    RequestHandler handler(cache);

    // create a http request
    std::string raw_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    HttpRequest request;
    request.parse_request(raw_request);

    // create a fake client socket, use the standard output
    int fake_socket = 1;

    handler.handle_request(request, fake_socket);

    std::cout << "RequestHandler Test Passed!\n";
}



int main() {
    test_http_request();
    test_http_response();
    test_request_handler();
    return 0;
}