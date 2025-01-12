#pragma once

#include <pthread.h>

#include <vector>

class Cache_reader;

class Cache_node {

    friend Cache_reader;

    public:

        Cache_node();

        ~Cache_node();

        // Thread-safe.
        int writeBytes(char* bytes, int length);

        // Thread-safe.
        void finalize();

        // Thread-safe.
        bool is_finalized();

        // Thread-safe.
        bool is_valid();

        // Thread-safe.
        void mark_as_invalid();

        // ! Thread-UNsafe
        int getAvaliableBytes();

        // ! Thread-UNsafe
        int getAvailableBytesFrom(int from);

        // Reads up to 'length' bytes to 'buffer'.
        // Returns number of bytes read. May return less than 'length' if less than 'length' bytes was available to read.
        // ! Thread-UNsafe
        int readFrom(int from, char* buffer, int length);

        // Provides newly created Cache_reader that allows safely perform reading operations with cache.
        // Must be deleted(call delete) after use.
        Cache_reader* new_reader();

    private:
        pthread_cond_t data_state_changed; // State considered as changed if new data has been written or node has been finilazed.
        // pthread_cond_t finalized;

        pthread_mutex_t mutex;
        std::vector<char>* data;

        int readers_count;

        bool is_finalized_flag;

        bool is_valid_flag;
        
        bool is_deleted_flag;

};