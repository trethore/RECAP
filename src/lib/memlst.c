#include "memlst.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MEMLST_GROW_INIT
#define MEMLST_GROW_INIT 4
#endif

#ifdef MEMLST_TRACE
#define MEMLST_LOG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define MEMLST_LOG(fmt, ...)
#endif

static int memlst_reserve(memlst_t *list, size_t min_capacity) {
    if (!list) return -1;
    if (list->cap >= min_capacity) return 0;

    size_t new_cap = list->cap ? list->cap : MEMLST_GROW_INIT;
    while (new_cap < min_capacity) {
        if (new_cap > (SIZE_MAX / 2)) {
            new_cap = min_capacity;
            break;
        }
        new_cap *= 2;
    }

    memlst_entry_t *new_entries = realloc(list->entries, new_cap * sizeof(*new_entries));
    if (!new_entries) {
        return -1;
    }

    list->entries = new_entries;
    list->cap = new_cap;
    return 0;
}

void memlst_init(memlst_t *list) {
    if (!list) return;
    list->entries = NULL;
    list->len = 0;
    list->cap = 0;
}

void *memlst_add_internal(memlst_t *list, dtor_fn dtor, void *ptr, const char *file, int line) {
#ifdef MEMLST_TRACE
    MEMLST_LOG("%s:%d memlst_add(%p, dtor=%p, ptr=%p)\n", file, line, (void *)list, (void *)dtor, ptr);
#endif
    (void)file;
    (void)line;
    if (!list) {
        if (dtor && ptr) dtor(ptr);
        errno = EINVAL;
        return NULL;
    }
    if (!ptr) return NULL;

#ifndef NDEBUG
    for (size_t i = 0; i < list->len; ++i) {
        assert(list->entries[i].ptr != ptr);
    }
#endif

    if (list->len == list->cap) {
        if (memlst_reserve(list, list->len + 1) != 0) {
            if (dtor) dtor(ptr);
            return NULL;
        }
    }

    list->entries[list->len].ptr = ptr;
    list->entries[list->len].dtor = dtor;
    list->len += 1;
    return ptr;
}

void memlst_collect_internal(memlst_t *list, const char *file, int line) {
#ifdef MEMLST_TRACE
    MEMLST_LOG("%s:%d memlst_collect(%p)\n", file, line, (void *)list);
#endif
    (void)file;
    (void)line;
    if (!list) return;

    for (size_t i = 0; i < list->len; ++i) {
        memlst_entry_t entry = list->entries[i];
        if (entry.dtor && entry.ptr) {
#ifdef MEMLST_TRACE
            MEMLST_LOG("  clean %p\n", entry.ptr);
#endif
            entry.dtor(entry.ptr);
        }
    }
    list->len = 0;
}

void memlst_destroy_internal(memlst_t *list, const char *file, int line) {
#ifdef MEMLST_TRACE
    MEMLST_LOG("%s:%d memlst_destroy(%p)\n", file, line, (void *)list);
#endif
    (void)file;
    (void)line;
    if (!list) return;
    memlst_collect_internal(list, file, line);
    free(list->entries);
    list->entries = NULL;
    list->cap = 0;
}

void memlst_remove_last(memlst_t *list) {
    if (!list || list->len == 0) return;
    size_t idx = list->len - 1;
    list->len = idx;
    list->entries[idx].ptr = NULL;
    list->entries[idx].dtor = NULL;
}
