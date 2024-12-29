#include "proxy-session.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

#include <codecvt>
#include <cstdint>
#include <iostream>
#include <locale>
#include <string>

#include <codecvt>

#include <cstring>
#include <exception>

#include <netdb.h>

#include <iostream>

Proxy_session::Proxy_session(int socket, Cache* cache)
{
    _client_socket = socket;
    _cache = cache;
}

Proxy_session::~Proxy_session()
{
    close(_client_socket);
    close(_host_socket);
}

void Proxy_session::open()
{

    print_connection_info();

    try {
        session_routine();
    }
    catch(std::runtime_error* e) {
        std::cerr << "[ERROR]: " << e->what() << std::endl;
    }

}

void Proxy_session::print_connection_info()
{
    sockaddr_in addr;
    socklen_t len;

    getpeername(_client_socket, (struct sockaddr *)&addr, &len);

    std::cout << "Opened session with: " << inet_ntoa(addr.sin_addr) << ":" << addr.sin_port << std::endl;
}

void Proxy_session::session_routine()
{
    HttpRequest *request = receive_http_request();

    if (request == nullptr)
    {
        throw new std::runtime_error("Unable to get request from client.");
    }

    if (request->port != 80) {
        delete request;
        throw new std::runtime_error("HTTP over TLS(HTTPS) is not supported");
    }
    
    std::cout << "Request url is: " << request->url << std::endl;

    if (_cache->contains(request->url)) {
        execute_http_request_from_cache(request);
    }
    else {
        execute_http_request(request);
    }

    delete request;

}

bool is_end_of_request(char* buff, std::size_t recv_size) {
    return buff[recv_size - 1] == '\n' && buff[recv_size - 3] == '\n';
}

Proxy_session::HttpRequest *Proxy_session::receive_http_request()
{

    const int buff_length = 4096;
    char *buff = new char[buff_length];

    std::stringstream raw;

    while (true)
    {

        int recv_size = recv(_client_socket, buff, buff_length, MSG_NOSIGNAL);

        if (recv_size == -1)
        {
            delete[] buff;
            return nullptr;
        }

        if (recv_size < 1)
        {
            break;
        }

        raw << std::string(buff, recv_size);

        if (is_end_of_request(buff, recv_size))
        {
            break;
        }
    }

    delete[] buff;

    return Proxy_session::HttpRequest::parse(raw.str(), true);
}

void set_address(in_addr* host_address, uint16_t port, sockaddr_in* addr) {
    (*addr).sin_family = AF_INET;
    (*addr).sin_port = htons(port);
    (*addr).sin_addr = *host_address;
}

void Proxy_session::execute_http_request(HttpRequest *request)
{

    std::cout << "[CACHE]: trying to create cache node" << std::endl;

    Cache::Cache_node* node = _cache->create_node(request->url);

    if (node == nullptr) { // In case if other thread already created node.
        std::cout << "[CACHE]: other thread has already created cache node. Going to get data via cache." << std::endl;
        execute_http_request_from_cache(request);
        return;
    }

    std::cout << "[CACHE]: cache node created." << std::endl;

    struct hostent *host = gethostbyname(request->host.c_str());
    if (host == nullptr)
    {
        node->mark_as_invalid();
        std::cerr << "Couldn't get hostname from: '" << request->host << "'" << std::endl; 
        return;
    }

    sockaddr_in addr;
    set_address((in_addr *)host->h_addr_list[0], request->port, &addr);

    bool connected = connect_to_host(&addr);

    if (!connected)
    {
        node->mark_as_invalid();
        std::cerr << "Couldn't connect to host." << std::endl;
        return;
    }

    try {
        send_http_request_to_host(request);
    }
    catch(std::runtime_error* err) {
        node->mark_as_invalid();
    }

    handle_host_response(node);

}

bool Proxy_session::connect_to_host(sockaddr_in *addr)
{
    _host_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in local_addr;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;

    int err = bind(_host_socket, (struct sockaddr *)&local_addr, sizeof(local_addr));

    if (err != 0)
    {
        return false;
    }

    if (_host_socket < 0)
    {
        return false;
    }

    std::cout << "Host socket is binded" << std::endl;
    return connect(_host_socket, (sockaddr *)addr, sizeof(*addr)) == 0;
}

void Proxy_session::send_http_request_to_host(Proxy_session::HttpRequest *request)
{
    size_t buff_size = 4096;
    char *buffer = new char[buff_size];
    size_t bytes_sent = 0;
    const char *request_bytes = request->raw.c_str();
    std::size_t total_length = std::strlen(request_bytes) + 1;
    while (bytes_sent < total_length)
    {
        size_t to_send = total_length - bytes_sent;
        if (to_send > buff_size)
        {
            to_send = buff_size;
        }
        std::memcpy(buffer, request_bytes + bytes_sent, to_send);

        int actual_sent = send(_host_socket, buffer, to_send, MSG_NOSIGNAL);

        if (actual_sent == -1)
        {
            std::cerr << strerror(errno) << std::endl;
            delete[] buffer;
            throw new std::runtime_error("Fail to send data." + std::string(strerror(errno)));
        }
        bytes_sent += actual_sent;
    }
    delete[] buffer;
}

void Proxy_session::handle_host_response(Cache::Cache_node* node)
{
    const int buff_length = 4096;
    char *buff = new char[buff_length];

    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);      

    while (true)
    {

        int recv_size = recv(_host_socket, buff, buff_length, MSG_NOSIGNAL);

        if (recv_size <= 0)
        {
            break;
        }

        int need_to_send = recv_size;

        while (need_to_send > 0)  {
            // std::cout << "Preparing to send: '" << recv_size - need_to_send << "' bytes to client." << std::endl;
            int sent_size = send(_client_socket, buff + (recv_size - need_to_send), need_to_send, MSG_NOSIGNAL);
            // std::cout << "Sent: '" << sent_size << "' bytes to client." << std::endl;
            if (sent_size <= 0) {
                node->mark_as_invalid();
                delete[] buff;
                return;
            }
            need_to_send -= sent_size;
        }

        int need_to_write = recv_size;
        while (need_to_write > 0) {
            int written_bytes = node->writeBytes(buff + (recv_size - need_to_write), need_to_write);
            need_to_write -= written_bytes;
        }

    }
    node->finalize();
    delete[] buff;
}

void Proxy_session::execute_http_request_from_cache(HttpRequest *request)
{

    std::cout << "[CACHE]: trying to get data" << std::endl;
    Cache::Cache_node* node = _cache->get(request->url);

    if (node == nullptr) {
        std::cout << "[CACHE]: data not found, going to execute request via network." << std::endl;
        execute_http_request(request);
        return;
    }

    std::cout << "[CACHE]: data found. Sending from cache." << std::endl;

    int read_begin = 0;

    int total_bytes_sent = 0;

    const int buffer_size = 4096;

    char* buffer = new char[buffer_size];

    while (node->is_valid() && (!node->is_finalized() || total_bytes_sent != node->getAvaliableBytes())) {        
        
        int need_to_read = node->getAvailableBytesFrom(read_begin);

        if (need_to_read == 0) {
            continue;
        }

        if (need_to_read > buffer_size) {
            need_to_read = buffer_size;
        }

        int read_bytes = node->readFrom(read_begin, buffer, need_to_read);

        read_begin += read_bytes;

        int need_to_send = read_bytes;

        while (need_to_send > 0)  {
            // std::cout << "Preparing to send: '" << read_bytes << "' bytes to client." << std::endl;
            int sent_size = send(_client_socket, buffer + (read_bytes - need_to_send), need_to_send, MSG_NOSIGNAL);
            // std::cout << "Sent: '" << sent_size << "' bytes to client." << std::endl;
            if (sent_size <= 0) {
                std::cerr << "[ERROR]: " << strerror(errno) << std::endl;
                delete[] buffer;
                return;
            }
            need_to_send -= sent_size;
        }

        total_bytes_sent += read_bytes;

    };
    
}
