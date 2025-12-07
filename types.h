#include <stddef.h>
#include <stdint.h>

typedef struct{
    char key[32];
    void *next;
    size_t offset;
} index_entry;

typedef struct{
    index_entry *arr;
    size_t items;
    size_t capacity;
    size_t max_capacity;
} ht;

ht * ht_alloc(size_t capacity, size_t max_capacity);
long ht_get(ht* table, const char* key);
int8_t ht_set(ht* table, const char* key, size_t value);
