#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"
#include "CacheManager.h"
#include "RequestHandler.h"
#include <cassert>
#include <iostream>

void test_http_request_parsing() {
    std::string raw_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "\r\n";

    HttpRequest request;
    bool parsed = request.parse_request(raw_request);
    
    assert(parsed);
    assert(request.get_method() == "GET");
    assert(request.get_url() == "/index.html");
    assert(request.get_host() == "example.com");
    assert(request.get_header("User-Agent") == "Mozilla/5.0");

    std::cout << "HttpRequest Parsing Test Passed!" << std::endl;
}

void test_http_response_parsing() {
    std::string raw_response =
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=3600\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "Hello, World!";

    HttpResponse response;
    bool parsed = response.parse_response(raw_response);

    
    //std::cout << "Expected: 'HTTP/1.1 200 OK' (" << std::string("HTTP/1.1 200 OK").length() << " characters)" << std::endl;
    //std::cout << "Actual: '" << response.get_status_line() << "' (" << response.get_status_line().length() << " characters)" << std::endl;

    //std::cout << "Expected: 'max-age=3600'" << std::endl;
    //std::cout << "Actual: '" << response.get_header("Cache-Control") << "' (Length: " 
    //      << response.get_header("Cache-Control").length() << " characters)" << std::endl;

    std::cout << "Expected Body: 'Hello, World!'" << std::endl;
    std::cout << "Actual Body: '" << response.get_body() << "' (Length: " 
          << response.get_body().length() << " characters)" << std::endl;

    assert(parsed);
    assert(response.get_status_line() == "HTTP/1.1 200 OK");
    assert(response.get_header("Cache-Control") == "max-age=3600");
    assert(response.get_body() == "Hello, World!");

    std::cout << "✅ HttpResponse Parsing Test Passed!" << std::endl;
}

void test_logger() {
    Logger& logger = Logger::get_instance();

    int request_id = 1;

    // Simulate a normal request logging
    logger.log_request(request_id, "GET /index.html HTTP/1.1", "127.0.0.1");

    logger.log_cache_status(request_id, "not in cache");

    // Simulate an expected error log
    //logger.log_error(request_id, "Test error message");

    // Simulate a normal response log
    logger.log_response(request_id, "HTTP/1.1 200 OK");
    std::cout << "✅ Logger Test Passed! (Check logs in /var/log/erss/proxy.log)" << std::endl;
}

void test_cache_manager() {
    CacheManager cache(5);
    std::shared_ptr<HttpResponse> response = std::make_shared<HttpResponse>("HTTP/1.1 200 OK");
    response->parse_response("HTTP/1.1 200 OK\r\nCache-Control: max-age=3600\r\n\r\n");
    int test_request_id = 42;  // Use a sample request ID for testing
    cache.store_response(test_request_id, "http://example.com", response);
    assert(cache.is_in_cache("http://example.com"));
    std::shared_ptr<HttpResponse> cached_response = cache.get_cached_response("http://example.com");
    assert(cached_response != nullptr);
    assert(cached_response->get_status_line() == "HTTP/1.1 200 OK");
    std::cout << "✅ CacheManager Test Passed!" << std::endl;
}

void test_request_handler_get() {
    CacheManager cache(5);
    RequestHandler handler(cache);
    HttpRequest request;
    std::string raw_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    request.parse_request(raw_request);
    int socket_fd = -1;  
    int request_id = 1;
    std::string client_ip = "127.0.0.1";
    int result = handler.handle_request(request, socket_fd, request_id, client_ip);
    assert(result == 0);
    std::cout << "✅ RequestHandler GET Test Passed!" << std::endl;
}

int main() {
    test_http_request_parsing();
    test_http_response_parsing();
    test_logger();
    test_cache_manager();
    test_request_handler_get();
    return 0;
}
