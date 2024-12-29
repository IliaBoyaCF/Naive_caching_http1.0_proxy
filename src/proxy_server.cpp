#include "proxy_server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#include <iostream>

Proxy_server::Proxy_server(int port)
{
    _server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (_server_socket == -1) {
        throw new std::runtime_error("Unable to create server socket");
    }

    sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    int err = bind(_server_socket, (struct sockaddr*)&addr, sizeof(addr));
    
    if (err) {
        std::string err_descr = strerror(errno);
        throw new std::runtime_error("Unable to bind server socket: '" + err_descr + "'.");
    }
    
    cache = new Cache();
}

Proxy_server::~Proxy_server()
{
    close(_server_socket);
}

void sigpipe_handler(int sig_id) {
    if (sig_id != SIGPIPE) {
        throw new std::runtime_error("Sigpipe handler called on other signal.");
    }
    std::cerr << "Got SIGPIPE. Ignored." << std::endl;
}

void Proxy_server::start()
{

    struct sigaction act;
    act.sa_handler = sigpipe_handler;
    sigaction(SIGPIPE, &act, NULL);

    listen(_server_socket, s_connection_queue_length);
    server_routine();
}

void* session_thread(void* args) {
    
    Proxy_session* session = (Proxy_session*)args;

    try {
        session->open();
    }

    catch (std::exception* e) {
        std::cerr << e->what() << std::endl;
    }
    
    delete session;
    
    return NULL;
}

void Proxy_server::server_routine()
{
    // timeval timeout;
    // timeout.tv_usec = 100000; // 0.1 second
    // fd_set readfd;

    pthread_attr_t detached_state_attrs;
    pthread_attr_init(&detached_state_attrs);
    pthread_attr_setdetachstate(&detached_state_attrs, PTHREAD_CREATE_DETACHED);

    while (true) {

        // FD_ZERO(&readfd);
        // FD_SET(_server_socket, &readfd);

        // int ready = select(_server_socket + 1, &readfd, nullptr, nullptr, &timeout);
        
        // if (ready < 1) {
        //     continue;
        // }

        int client_socket = accept(_server_socket, nullptr, nullptr);

        if (client_socket == -1) {
            std::cerr << strerror(errno) << std::endl;
            continue;
        }
        
        pthread_t tid;
        pthread_create(&tid, &detached_state_attrs, session_thread, new Proxy_session(client_socket, cache));

    }
}
