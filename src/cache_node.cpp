#include "cache_node.hpp"
#include "cache_reader.hpp"

#include <exception>
#include <iostream>

Cache_node::Cache_node()
{
    int err = pthread_mutex_init(&mutex, nullptr);

    if (err) {
        throw new std::runtime_error("Couldn't initialize mutex.");
    }
    
    err = pthread_cond_init(&data_state_changed, nullptr);
    
    if (err) {
        pthread_mutex_destroy(&mutex);
        throw new std::runtime_error("Couldn't initialize conditional variable.");
    }

    data = new std::vector<char>();
    
    is_finalized_flag = false;
    is_valid_flag = true;
    is_deleted_flag = false;
    
    readers_count = 0;
}

Cache_node::~Cache_node()
{
    
    is_deleted_flag = true;

    if (readers_count > 0) {
        return;
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&data_state_changed);

    delete data;
}

int Cache_node::writeBytes(char *bytes, int length)
{
    int bytes_written;

    pthread_mutex_lock(&mutex);

    if (!is_valid_flag) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    
    int length_before = data->size();
    for (int i = 0; i < length; ++i) {
        data->push_back(bytes[i]);
    }

    bytes_written = data->size() - length_before;
    
    pthread_cond_broadcast(&data_state_changed);

    pthread_mutex_unlock(&mutex);

    return bytes_written;
}

void Cache_node::finalize()
{
    pthread_mutex_lock(&mutex);
    is_finalized_flag = true;
    std::cout << "[CACHE]: Node is finalized. Size is: " << data->size() << std::endl;
    pthread_cond_broadcast(&data_state_changed);
    pthread_mutex_unlock(&mutex);
}

bool Cache_node::is_finalized()
{
    return is_finalized_flag;
}

bool Cache_node::is_valid()
{
    return is_valid_flag;
}

void Cache_node::mark_as_invalid()
{
    pthread_mutex_lock(&mutex);
    is_valid_flag = false;
    is_deleted_flag = true;
    pthread_mutex_unlock(&mutex);
}

int Cache_node::getAvaliableBytes()
{
    int available;
    available = data->size();
    return available;
}

int Cache_node::getAvailableBytesFrom(int from)
{
    int res = getAvaliableBytes() - from;
    if (res < 0) {
        res = 0;
    }
    return res;
}

int Cache_node::readFrom(int from, char *buffer, int length)
{
    int available_length = getAvailableBytesFrom(from);
    
    if (available_length == 0) {
        return 0;
    }

    int can_read = length;

    if (available_length < length) {
        can_read = available_length;
    }

    for (int i = from; i < from + can_read; ++i) {
        buffer[i - from] = data->at(i);
    }
    
    return can_read;
}

Cache_reader *Cache_node::new_reader()
{
    pthread_mutex_lock(&mutex);
    readers_count += 1;
    pthread_mutex_unlock(&mutex);
    return new Cache_reader(this);
}
