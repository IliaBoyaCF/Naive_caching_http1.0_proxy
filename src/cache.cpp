#include "cache.hpp"

#include <iostream>

static Cache_node* unsynchronized_get(std::string request, std::unordered_map<std::string, Cache_node*>* cache);

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
    
    Cache_node* node = unsynchronized_get(request, cache);

    pthread_mutex_unlock(&mutex);

    return node;
}

Cache_node* unsynchronized_get(std::string request, std::unordered_map<std::string, Cache_node*>* cache) {

    auto search = cache->find(request);
    auto end = cache->end();

    if (search != end && !search->second->is_valid()) {
        cache->erase(request);
        delete search->second;
        search = end;
    }

    if (search == end) {
        return nullptr;
    }

    return search->second;
}

Cache_node *Cache::create_node(std::string keyRequest)
{
    pthread_mutex_lock(&mutex);
    Cache_node* res = unsynchronized_get(keyRequest, cache);
    if (res != nullptr) {   // node with given key is already exists, so return nullptr
        res = nullptr;
    }
    else {
        res = new Cache_node();
        cache->insert(std::pair<std::string, Cache_node*>(keyRequest, res));
    }
    pthread_mutex_unlock(&mutex);
    return res;
}

void Cache::delete_node(std::string request)
{
    pthread_mutex_lock(&mutex);
    auto search = cache->find(request);
    if (search != cache->end()) {
        delete search->second;
        cache->erase(request);
    }
    pthread_mutex_unlock(&mutex);
}

void Cache::clear()
{
    pthread_mutex_lock(&mutex);
    cache->clear();
    pthread_mutex_unlock(&mutex);
}
