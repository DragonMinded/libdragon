#ifndef _GL_OBJ_MAP_H
#define _GL_OBJ_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>

typedef struct {
    uint32_t key;
    void *value;
} obj_map_entry_t;

typedef struct {
    obj_map_entry_t *entries;
    uint32_t capacity;
    uint32_t count;
} obj_map_t;

typedef struct {
    uint32_t key;
    void *value;

    obj_map_t *_map;
    uint32_t _index;
} obj_map_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

void obj_map_new(obj_map_t *map);
void obj_map_free(obj_map_t *map);

inline uint32_t obj_map_count(const obj_map_t *map)
{
    assertf(map->entries != NULL, "Map is not initialized!");

    return map->count;
}

void * obj_map_get(const obj_map_t *map, uint32_t key);
void * obj_map_set(obj_map_t *map, uint32_t key, void *value);
void * obj_map_remove(obj_map_t *map, uint32_t key);

obj_map_iter_t obj_map_iterator(obj_map_t *map);
bool obj_map_iterator_next(obj_map_iter_t *iter);

#ifdef __cplusplus
}
#endif


#endif
