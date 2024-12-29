#pragma once

#include "cache.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <functional>
#include <pthread.h>
#include <cstring>

#include <iostream>

class Proxy_session {
    public:
        Proxy_session(int socket, Cache* cache);
        ~Proxy_session();

        void open();

    private:
        struct HttpRequest {

            std::string host;
            uint16_t port;
            std::string raw;
            std::string url;

            HttpRequest(std::string host, std::string raw, uint16_t port, std::string url) {
                this->host = host;
                this->raw = raw;
                this->port = port;
                this->url = url;
            }

            static HttpRequest* parse(std::string raw, bool change_connection_to_close) {
                if (raw.empty()) {
                    return nullptr;
                }

                if (change_connection_to_close) {
                    raw = change_connection_header_to_close(raw);
                }

                std::string host = get_host(raw);

                std::string url = parse_url(raw);

                uint16_t port = parse_port(raw);                        

                return new HttpRequest(host, raw, port, url);

                // uint16_t port = 0;
                // if (hostname_end > full_name_end) {
                //     port = 80;
                //     return new HttpRequest(host, raw, port, url);
                // }
                // else {
                //     uint16_t port = std::stoi(raw.substr(hostname_end + 1, full_name_end - hostname_end));
                //     return new HttpRequest(host, raw, port, url);    
                // }
            }

            static uint16_t parse_port(std::string request) {

                std::string host_header = "Host: ";
                std::size_t begin = request.find(host_header) + host_header.length();
                std::size_t hostname_end = request.find(":", begin);
                std::size_t full_name_end = request.find("\r\n", begin);

                if (hostname_end > full_name_end) {
                    return 80;
                }
                else {
                    return std::stoi(request.substr(hostname_end + 1, full_name_end - hostname_end));
                }

            }

            static std::string get_host(std::string request) {

                std::string host_header = "Host: ";
                std::size_t begin = request.find(host_header) + host_header.length();
                std::size_t hostname_end = request.find(":", begin);
                std::size_t full_name_end = request.find("\r\n", begin);
                
                if (hostname_end > full_name_end) {
                    return request.substr(begin, full_name_end - begin);
                }

                return request.substr(begin, hostname_end - begin);
            }

            static std::string change_connection_header_to_close(std::string raw) {
                const std::string connection_header = "Connection: ";
                
                std::string new_value = "close";

                std::size_t connection_value_start = raw.find(connection_header);

                if (connection_value_start == std::string::npos) {
                    std::cerr << "Unable to find 'Connection:' header." << std::endl;
                    return raw;
                }

                connection_value_start += connection_header.length();

                std::size_t value_end = raw.find("\r\n", connection_value_start);

                if (value_end == std::string::npos) {
                    std::cerr << "Unable to find end of 'Connection:' header value." << std::endl;
                    return raw;
                }

                return raw.replace(connection_value_start, value_end - connection_value_start, new_value);
                
            }

            static std::string parse_url(std::string request) {
                std::size_t url_begin = request.find("GET http://");

                std::size_t url_end = request.find("HTTP", url_begin);
                url_end -= 1;

                std::string res;

                if (url_begin == std::string::npos || url_begin > url_end) {
                    url_begin = request.find("/");
                    res = get_host(request) + request.substr(url_begin, url_end - url_begin);
                }
                else {
                    url_begin += strlen("GET http://");
                    res = request.substr(url_begin, url_end - url_begin);
                }
                
                return res;
            }

        };

        int _client_socket;
        int _host_socket;

        pthread_t tid;

        Cache* _cache;

        void print_connection_info();

        void session_routine();
        
        HttpRequest* receive_http_request();
        bool connect_to_host(sockaddr_in* addr);
        void send_http_request_to_host(HttpRequest* request);
        void execute_http_request(HttpRequest* request);
        void handle_host_response(Cache::Cache_node* node);
        void execute_http_request_from_cache(HttpRequest* request);
};
