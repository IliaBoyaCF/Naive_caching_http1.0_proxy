#pragma once

#include <exception>
#include <string>

#include <unordered_map>
#include <vector>

#include <pthread.h>

class Cache {
    public:
        class Cache_node {
            public:

                Cache_node();

                ~Cache_node();

                int writeBytes(char* bytes, int length);

                void finalize();

                bool is_finalized();

                bool is_valid();

                void mark_as_invalid();

                int getAvaliableBytes();

                int getAvailableBytesFrom(int from);

                // Reads up to 'length' bytes to 'buffer'.
                // Returns number of bytes read. May return less than 'length' if less than 'length' bytes was available to read.
                int readFrom(int from, char* buffer, int length);

            private:
                pthread_cond_t data_available;
                pthread_cond_t finalized;

                pthread_mutex_t mutex;
                std::vector<char>* data;

                bool is_finalized_flag;  
                bool is_valid_flag; 

        };

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

        void clear();

    private:
        std::unordered_map<std::string, Cache_node*>* cache;

        pthread_mutex_t mutex;
};
