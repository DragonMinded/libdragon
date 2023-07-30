/**
 * This is a very simple hash map that uses open adressing (linear probing).
 * The hash function is the identity for now, since it uses integer keys.
 */

#include "obj_map.h"

#include <malloc.h>
#include <debug.h>
#include <stdlib.h>

#define OBJ_MAP_MIN_CAPACITY     32
#define OBJ_MAP_DELETED_KEY      0xFFFFFFFF

void obj_map_new(obj_map_t *map)
{
    assertf(map->entries == NULL, "Map has not been freed!");

    map->entries = calloc(OBJ_MAP_MIN_CAPACITY, sizeof(obj_map_entry_t));
    map->capacity = OBJ_MAP_MIN_CAPACITY;
    map->count = 0;
}

void obj_map_free(obj_map_t *map)
{
    assertf(map->entries != NULL, "Map is not initialized!");

    free(map->entries);
    map->entries = NULL;
}

obj_map_entry_t * obj_map_find_entry(const obj_map_t *map, uint32_t key)
{
    uint32_t mask = (map->capacity - 1);

    for (uint32_t i = 0; i < map->capacity; i++) {
        obj_map_entry_t *entry = &map->entries[(key + i) & mask];

        if (entry->key == key) {
            return entry;
        }

        if (entry->value == NULL && entry->key != OBJ_MAP_DELETED_KEY) {
            // Stop probing when unused entry is found.
            // However, keep searching if the entry has been deleted
            break;
        }
    }

    return NULL;
}

void * obj_map_set_without_expanding(obj_map_t *map, uint32_t key, void *value)
{
    uint32_t mask = (map->capacity - 1);

    for (uint32_t i = 0; i < map->capacity; i++) {
        obj_map_entry_t *e = &map->entries[(key + i) & mask];

        if (e->value == NULL) {
            // Entry is unused or has been deleted
            // -> New entry is added
            e->key = key;
            e->value = value;
            map->count++;
            return NULL;
        }

        if (e->key == key) {
            // Key is already present
            // -> Value is changed, but no new entry is added
            void *old_value = e->value;
            e->value = value;
            return old_value;
        }
    }

    assertf(0, "Map is full!");
    abort();
}

void obj_map_expand(obj_map_t *map)
{
    obj_map_entry_t *old_entries = map->entries;
    uint32_t old_capacity = map->capacity;

    map->capacity = old_capacity << 1;
    map->entries = calloc(map->capacity, sizeof(obj_map_entry_t));
    map->count = 0;

    // Re-populate the map with all used entries
    for (uint32_t i = 0; i < old_capacity; i++) {
        obj_map_entry_t *entry = &old_entries[i];
        if (entry->value != NULL) {
            obj_map_set_without_expanding(map, entry->key, entry->value);
        }
    }

    free(old_entries);
}

void * obj_map_get(const obj_map_t *map, uint32_t key)
{
    assertf(map->entries != NULL, "Map is not initialized!");

    obj_map_entry_t *entry = obj_map_find_entry(map, key);
    return entry == NULL ? NULL : entry->value;
}

void * obj_map_set(obj_map_t *map, uint32_t key, void *value)
{
    assertf(map->entries != NULL, "Map is not initialized!");
    assertf(value != NULL, "Can't insert NULL into map!");

    if (map->count * 2 > map->capacity) {
        // If more than half the capacity is used, expand the map
        obj_map_expand(map);
    }

    return obj_map_set_without_expanding(map, key, value);
}

void * obj_map_remove(obj_map_t *map, uint32_t key)
{
    assertf(map->entries != NULL, "Map is not initialized!");

    obj_map_entry_t *entry = obj_map_find_entry(map, key);

    if (entry != NULL) {
        void *v = entry->value;
        entry->value = NULL;
        // Mark the entry as deleted with magic value
        entry->key = OBJ_MAP_DELETED_KEY;
        map->count--;
        return v;
    }

    return NULL;
}

obj_map_iter_t obj_map_iterator(obj_map_t *map)
{
    assertf(map->entries != NULL, "Map is not initialized!");

    return (obj_map_iter_t) {
        ._map = map,
        ._index = 0
    };
}

bool obj_map_iterator_next(obj_map_iter_t *iter)
{
    assertf(iter->_map != NULL, "Map iterator is not initialized!");

    uint32_t cur_index = iter->_index;

    while (cur_index < iter->_map->capacity) {
        obj_map_entry_t *cur_entry = &iter->_map->entries[cur_index];
        cur_index++;

        if (cur_entry->value != NULL) {
            iter->key = cur_entry->key;
            iter->value = cur_entry->value;
            iter->_index = cur_index;
            return true;
        }
    }

    return false;
}
