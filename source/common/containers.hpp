#pragma once

#include "tools.hpp"

// Simple hash table implementation
template <
    typename T,
    uint32_t Bucket_Count,
    uint32_t Bucket_Size,
    uint32_t Bucket_Search_Count> struct hash_table_t {
    enum { UNINITIALIZED_HASH = 0xFFFFFFFF };
    enum { ITEM_POUR_LIMIT    = Bucket_Search_Count };

    struct item_t {
        uint32_t hash = UNINITIALIZED_HASH;
        T value = T();
    };

    struct bucket_t {
        uint32_t bucket_usage_count = 0;
        item_t items[Bucket_Size] = {};
    };

    bucket_t buckets[Bucket_Count] = {};

    void init() {
        for (uint32_t bucket_index = 0; bucket_index < Bucket_Count; ++bucket_index) {
            for (uint32_t item_index = 0; item_index < Bucket_Size; ++item_index) {
                buckets[bucket_index].items[item_index].hash = UNINITIALIZED_HASH;
            }
        }
    }

    void clean_up() {
        for (uint32_t i = 0; i < Bucket_Count; ++i) {
            buckets[i].bucket_usage_count = 0;
        }
    }

    void insert(
        uint32_t hash,
        T value) {
        bucket_t *bucket = &buckets[hash % Bucket_Count];

        for (uint32_t bucket_item = 0; bucket_item < Bucket_Size; ++bucket_item) {
            item_t *item = &bucket->items[bucket_item];
            if (item->hash == UNINITIALIZED_HASH) {
                /* found a slot for the object */
                item->hash = hash;
                item->value = value;
                return;
            }
        }
        
        assert(false);
    }
    
    void remove(
        uint32_t hash) {
        bucket_t *bucket = &buckets[hash % Bucket_Count];

        for (uint32_t bucket_item = 0; bucket_item < Bucket_Size; ++bucket_item) {

            item_t *item = &bucket->items[bucket_item];

            if (item->hash == hash && item->hash != UNINITIALIZED_HASH) {
                item->hash = UNINITIALIZED_HASH;
                item->value = T();
                return;
            }
        }

        assert(false);
    }
    
    T *get(
        uint32_t hash) {
        static int32_t invalid = -1;
        
        bucket_t *bucket = &buckets[hash % Bucket_Count];
        
        uint32_t bucket_item = 0;

        uint32_t filled_items = 0;

        for (; bucket_item < Bucket_Size; ++bucket_item) {
            item_t *item = &bucket->items[bucket_item];
            if (item->hash != UNINITIALIZED_HASH) {
                ++filled_items;
                if (hash == item->hash) {
                    return(&item->value);
                }
            }
        }

        if (filled_items == Bucket_Size) {
            assert(false);
        }

        return NULL;
    }
};

template <
    typename T> struct stack_container_t {
    uint32_t max_size = 0;
    uint32_t data_count = 0;
    T *data;

    uint32_t removed_count = 0;
    uint32_t *removed;

    void init(
        uint32_t max) {
        max_size = max;
        data = FL_MALLOC(T, max_size);
        removed = FL_MALLOC(uint32_t, max_size);
    }

    void destroy() {
        FL_FREE(data);
        FL_FREE(removed);
    }
    
    uint32_t add() {
        if (removed_count) {
            return removed[removed_count-- - 1];
        }
        else {
            return data_count++;
        }
    }

    T *get(
        uint32_t index) {
        return &data[index];
    }

    T &operator[](
        uint32_t i) {
        return data[i];
    }

    void remove(
        uint32_t index) {
        data[index] = T{};
        removed[removed_count++] = index;
    }
};
