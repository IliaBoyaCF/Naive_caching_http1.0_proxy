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

Cache::Cache_node *Cache::get(std::string request)
{
    pthread_mutex_lock(&mutex);
    auto search = cache->find(request);
    auto end = cache->end();
    pthread_mutex_unlock(&mutex);

    if (search == end) {
        return nullptr;
    }

    return search->second;
}

Cache::Cache_node *Cache::create_node(std::string keyRequest)
{
    Cache_node* res;
    pthread_mutex_lock(&mutex);
    if (cache->find(keyRequest) != cache->end()) {
        res = nullptr;
    }
    else {
        res = new Cache_node();
    }
    cache->insert(std::pair<std::string, Cache::Cache_node*>(keyRequest, res));
    pthread_mutex_unlock(&mutex);
    return res;
}

void Cache::clear()
{
    pthread_mutex_lock(&mutex);
    cache->clear();
    pthread_mutex_unlock(&mutex);
}

Cache::Cache_node::Cache_node()
{
    int err = pthread_mutex_init(&mutex, nullptr);

    if (err) {
        throw new std::runtime_error("Couldn't initialize mutex.");
    }
    
    err = pthread_cond_init(&data_available, nullptr);
    
    if (err) {
        pthread_mutex_destroy(&mutex);
        throw new std::runtime_error("Couldn't initialize mutex.");
    }

    err = pthread_cond_init(&finalized, nullptr);
    
    if (err) {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&data_available);
        throw new std::runtime_error("Couldn't initialize mutex.");
    }

    data = new std::vector<char>();
    
    is_finalized_flag = false;
}

Cache::Cache_node::~Cache_node()
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&data_available);
    pthread_cond_destroy(&finalized);
    delete data;
}

int Cache::Cache_node::writeBytes(char *bytes, int length)
{
    int bytes_written;

    pthread_mutex_lock(&mutex);
    
    int length_before = data->size();
    for (int i = 0; i < length; ++i) {
        data->push_back(bytes[i]);
    }

    bytes_written = data->size() - length_before;
    
    pthread_cond_broadcast(&data_available);

    pthread_mutex_unlock(&mutex);

    return bytes_written;
}

void Cache::Cache_node::finalize()
{
    pthread_mutex_lock(&mutex);
    is_finalized_flag = true;
    std::cout << "[CACHE]: Node is finalized. Size is: " << data->size() << std::endl;
    pthread_cond_broadcast(&data_available);
    pthread_mutex_unlock(&mutex);
}

bool Cache::Cache_node::is_finalized()
{
    return is_finalized_flag;
}

int Cache::Cache_node::getAvaliableBytes()
{
    int available;
    pthread_mutex_lock(&mutex);
    available = data->size();
    pthread_mutex_unlock(&mutex);
    return available;
}

int Cache::Cache_node::getAvailableBytesFrom(int from)
{
    int res = getAvaliableBytes() - from;
    if (res < 0) {
        res = 0;
    }
    return res;
}

int Cache::Cache_node::readFrom(int from, char *buffer, int length)
{
    int available_length = getAvailableBytesFrom(from);
    
    if (available_length == 0) {
        return 0;
    }

    int can_read = length;

    if (available_length < length) {
        can_read = available_length;
    }

    pthread_mutex_lock(&mutex);

    for (int i = from; i < from + can_read; ++i) {
        buffer[i - from] = data->at(i);
    }

    pthread_mutex_unlock(&mutex);
    
    return can_read;
}
