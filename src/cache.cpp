#include "cache.hpp"

#include <iostream>

Cache::Cache()
{
    cache = new std::unordered_map<std::string, Cache_node*>();

    int err = pthread_mutex_init(&mutex, nullptr);

    if (err) {
        throw new std::runtime_error("Couldn't initialize mutex");
    }

}

Cache::~Cache()
{
    delete cache;
    pthread_mutex_destroy(&mutex);
}

bool Cache::contains(std::string request)
{
    pthread_mutex_lock(&mutex);
    bool res = cache->find(request) != cache->end();
    pthread_mutex_unlock(&mutex);
    return  res;
}

Cache_node *Cache::get(std::string request)
{
    pthread_mutex_lock(&mutex);
    auto search = cache->find(request);
    auto end = cache->end();

    if (search != end && !search->second->is_valid()) {
        cache->erase(request);
        search = end;
    }

    pthread_mutex_unlock(&mutex);

    if (search == end) {
        return nullptr;
    }

    return search->second;
}

Cache_node *Cache::create_node(std::string keyRequest)
{
    Cache_node* res;
    if (get(keyRequest) != nullptr) {
        res = nullptr;
    }
    else {
        res = new Cache_node();
        pthread_mutex_lock(&mutex);
        cache->insert(std::pair<std::string, Cache_node*>(keyRequest, res));
        pthread_mutex_unlock(&mutex);
    }
    return res;
}

void Cache::clear()
{
    pthread_mutex_lock(&mutex);
    cache->clear();
    pthread_mutex_unlock(&mutex);
}
