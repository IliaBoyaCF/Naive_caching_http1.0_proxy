#pragma once

#include "proxy-session.hpp"

#include <list>
#include <functional>
#include <pthread.h>

class Proxy_server {
    public:
        Proxy_server(int port);
        
        ~Proxy_server();

        void start();

    private:

        static const int s_connection_queue_length = 10;

        int _server_socket;

        Cache* cache;

        void server_routine();
};
