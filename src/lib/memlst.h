#ifndef MEMLST_H
#define MEMLST_H

#include <stddef.h>

typedef void (*dtor_fn)(void *);

typedef struct memlst_entry {
    void *ptr;
    dtor_fn dtor;
} memlst_entry_t;

typedef struct memlst {
    memlst_entry_t *entries;
    size_t len;
    size_t cap;
} memlst_t;

void memlst_init(memlst_t *list);

void memlst_destroy_internal(memlst_t *list, const char *file, int line);
void memlst_collect_internal(memlst_t *list, const char *file, int line);
void *memlst_add_internal(memlst_t *list, dtor_fn dtor, void *ptr, const char *file, int line);
void memlst_remove_last(memlst_t *list);

#define memlst_destroy(list) memlst_destroy_internal((list), __FILE__, __LINE__)
#define memlst_collect(list) memlst_collect_internal((list), __FILE__, __LINE__)
#define memlst_add(list, dtor, ptr) memlst_add_internal((list), (dtor), (ptr), __FILE__, __LINE__)

#endif // MEMLST_H
