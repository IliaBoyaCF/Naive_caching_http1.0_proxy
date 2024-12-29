#include "proxy_server.hpp"

#include <iostream>

int main() {

    try {
        Proxy_server server(12345);
        
        std::cout << "----Server started----" << std::endl;

        server.start();

        return 0;
    }
    catch (std::runtime_error* err) {
        std::cout << err->what() << std::endl;
        return 1;
    }
}
