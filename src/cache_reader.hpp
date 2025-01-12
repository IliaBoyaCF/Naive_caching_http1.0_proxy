#pragma once

class Cache_node;

class Cache_reader {
    public:

        Cache_reader(Cache_node* node);

        ~Cache_reader();

        bool has_next();

        int avaliable();

        int read(char* buffer, int length);

        int skip(int length);

    private:

        Cache_node* node;

        int next_byte_to_read;

        // bool valid;

};
