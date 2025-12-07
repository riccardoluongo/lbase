// Functions for the in-memory hash map of indexes

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

static uint64_t hash_key(const char* key) {
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

// Allocate space for hash table struct
ht* ht_alloc(size_t capacity, size_t max_capacity){
    ht* table = malloc(sizeof(ht));
    if (table == NULL)
        return NULL;

    table->items = 0;
    table->capacity = capacity;
    table->max_capacity = max_capacity;

    if((table->arr = calloc(table->capacity, sizeof(index_entry))) == NULL)
        return NULL;

    return table;
}

// Look for an entry in the cache hash table and return a pointer its value if it exists, otherwise -1
long ht_get(ht* table, const char* key){
    size_t i = hash_key(key) % table->capacity;
    index_entry *entry;

    for(entry = &table->arr[i]; strcmp(key, table->arr[i].key) != 0; entry = (index_entry *)table->arr[i].next)
        if(entry == NULL)
            return -1;

    return entry->offset;
}

/*
Sets an entry in the cache hash table
returns 0 on success, -1 on capacity overflows and -2 on memory allocation errors
*/
int8_t ht_set(ht* table, const char* key, size_t value){
    // If length will exceed half of current capacity, expand it.

    if(table->items >= table->capacity / 1.5){
        size_t new_capacity = table->capacity * 2;
        if (new_capacity < table->capacity || new_capacity > table->max_capacity)
            return -1;

        if(realloc(table, new_capacity) == NULL)
            return -2;
    }

    uint64_t i = hash_key(key) % table->capacity;
    index_entry *entry = &table->arr[i];

    if(strcmp(entry->key, "") == 0){ // entry is empty, use it
        strcpy(entry->key, key);
        entry->offset = value;
    } else if(strcmp(entry->key, key) == 0){ // entry already exists for given key, update it
        entry->offset = value;
    } else{ // collision found
        for(entry = table->arr[i].next; entry != NULL; entry = entry->next) //skip used entries
            ;

        strcpy(entry->key, key);
        entry->offset = value;
    }

    table->items++;

    return 0;
}