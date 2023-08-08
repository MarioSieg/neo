/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Gargabe collector. */

#ifndef NEO_GC_H
#define NEO_GC_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** The current GC is a conservative, thread local, mark and sweep garbage collector.
** It could be replaced by a faster generational, concurrent, compacting garbage collector but this requires a lot of work.
** The current GC is simple and works.
**
** A memory allocation is considered reachable by the GC if...
** -> A pointer points to it, located on the VM stack.
** -> A pointer points to it, inside memory allocated by gc_alloc and friends.
** Otherwise, a memory allocation is considered unreachable.
**
** Therefore, some things that don't qualify an allocation as reachable are, if...
** -> A pointer points to an address inside of it, but not at the start of it.
** -> A pointer points to it from inside the static data segment.
** -> A pointer points to it from memory allocated by malloc, calloc, realloc or any other non-GC allocation methods.
** -> A pointer points to it from a different thread.
** -> A pointer points to it from any other unreachable location.
**
** Given these conditions, tgc will free memory allocations some time after they become unreachable.
** To do this, it performs an iteration of mark and sweep when gc_vmalloc is called and the number of memory allocations exceeds some threshold.
** It can also be run manually with gc_collect.
*/

#define GC_LOADFACTOR 0.9 /* GC must be 90 % full before resizing. */
#define GC_SWEEPFACTOR 0.5 /* Trigger a sweep when the number of allocated items exceeds 50% of the maximum capacity. */

#define gc_hash(p) ((uintptr_t)(p)>>3)

typedef enum gc_flags_t {
    GCF_NONE = 0,
    GCF_MARK = 1 << 0,
    GCF_ROOT = 1 << 1,
    GCF_LEAF = 1 << 2
} gc_flags_t;

typedef struct gc_fatptr_t {
    void *ptr;
    gc_flags_t flags : 8;
    size_t size;
    size_t hash;
    void (*dtor)(void*);
} gc_fatptr_t;

typedef struct gc_context_t {
    void *stk_top;
    void *stk_bot;
    bool paused;
    uintptr_t minptr;
    uintptr_t maxptr;
    gc_fatptr_t *items;
    gc_fatptr_t *frees;
    double loadfactor;
    double sweepfactor;
    size_t nitems;
    size_t nslots;
    size_t mitems;
    size_t nfrees;
} gc_context_t;

extern NEO_EXPORT void gc_init(gc_context_t *self, void *stk_top, void *stk_bot);
extern NEO_EXPORT void gc_free(gc_context_t *self);
extern NEO_EXPORT void gc_pause(gc_context_t *self);
extern NEO_EXPORT void gc_resume(gc_context_t *self);
extern NEO_EXPORT void gc_collect(gc_context_t *self);
extern NEO_EXPORT gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, void *ptr);
extern NEO_EXPORT void *gc_alloc(gc_context_t *self, size_t size, void(*dtor)(void *), bool is_root);
#define gc_vmalloc(self, size, dtor) gc_alloc((self),(size),(dtor),false)
#define gc_vmalloc_root(self, size, dtor) gc_alloc((self),(size),(dtor),true)
extern NEO_EXPORT void gc_set_dtor(gc_context_t *self, void *ptr, void(*dtor)(void*));
extern NEO_EXPORT void gc_set_flags(gc_context_t *self, void *ptr, gc_flags_t flags);
extern NEO_EXPORT gc_flags_t gc_get_flags(gc_context_t *self, void *ptr);
extern NEO_EXPORT void(*gc_get_dtor(gc_context_t *self, void *ptr))(void*);
extern NEO_EXPORT size_t gc_get_size(gc_context_t *self, void *ptr);

#ifdef __cplusplus
}
#endif

#endif
