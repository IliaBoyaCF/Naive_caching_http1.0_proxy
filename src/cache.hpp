#pragma once

#include "cache_node.hpp"

#include <exception>
#include <string>

#include <unordered_map>
#include <set>
#include <vector>

#include <pthread.h>

class Cache {
    public:

        Cache();

        ~Cache();

        // Returns true if cache node assosiated with given key exists. In other cases returns false.
        bool contains(std::string request);

        // Returns pointer to cache node assosiated with given key if assosiation exists.
        // In other cases returns nullptr.
        Cache_node* get(std::string request);

        // Returns pointer to newly created node assosiated with keyRequest.
        // Returns nullptr if node assosiated with given key already exists.
        Cache_node* create_node(std::string keyRequest);

        void delete_node(std::string request);

        void clear();

    private:
        std::unordered_map<std::string, Cache_node*>* cache;

        pthread_mutex_t mutex;
};
