#include "cache_reader.hpp"

#include "cache_node.hpp"

Cache_reader::Cache_reader(Cache_node *node)
{
    this->node = node;
    next_byte_to_read = 0;
    valid = true;
}

Cache_reader::~Cache_reader()
{
    pthread_mutex_lock(&node->mutex);
    node->readers_count -= 1;
    bool need_to_delete = node->readers_count == 0 && node->is_deleted_flag;
    pthread_mutex_unlock(&node->mutex);
    if (need_to_delete) {
        delete node;
    }
}

bool Cache_reader::has_next()
{
    if (!valid) {
        return false;
    }

    pthread_mutex_lock(&node->mutex);

    bool avaliable = node->getAvailableBytesFrom(next_byte_to_read) > 0;
    while (valid && !avaliable && !node->is_finalized()) {
        pthread_cond_wait(&node->data_state_changed, &node->mutex);
        avaliable = node->getAvailableBytesFrom(next_byte_to_read) > 0;
    }

    pthread_mutex_unlock(&node->mutex);

    return avaliable;
}

void Cache_reader::mark_as_invalid()
{
    valid = false;
}

bool Cache_reader::is_valid()
{
    return valid;
}

int Cache_reader::avaliable()
{
    pthread_mutex_lock(&node->mutex);

    int avaliable = node->getAvailableBytesFrom(next_byte_to_read);

    pthread_mutex_unlock(&node->mutex);

    return avaliable;
}

int Cache_reader::read(char *buffer, int length)
{
    pthread_mutex_lock(&node->mutex);

    int need_to_read = length;

    while (node->is_valid() && need_to_read > 0) {
        int read_bytes = node->readFrom(next_byte_to_read, buffer + (length - need_to_read), need_to_read);
        next_byte_to_read += read_bytes;
        need_to_read -= read_bytes;
        if (need_to_read > 0) 
        {
            if (!node->is_finalized()) {
                pthread_cond_wait(&node->data_state_changed, &node->mutex);
            }
            else {
                break;
            }
        }
    }

    pthread_mutex_unlock(&node->mutex);

    return length - need_to_read;

}

int Cache_reader::skip(int length)
{
    pthread_mutex_lock(&node->mutex);

    int skipped = node->getAvailableBytesFrom(next_byte_to_read);

    if (skipped > length) {
        skipped = length;
    }

    next_byte_to_read += skipped;

    pthread_mutex_unlock(&node->mutex);

    return skipped;
}
