#include <iostream>
#include <exception>
#include "ProxyServer.h"

#define PROXY_SERVER_PORT 80

int main() {
    try {
        ProxyServer proxy(PROXY_SERVER_PORT);
        proxy.start(); //blocking call
    } catch (const std::exception& e) { //catch all errors
        std::cout << "Exception caught: " << e.what() << std::endl << "Shutting down proxy server..." << std::endl;
    }

    return 0;
}