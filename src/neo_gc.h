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

typedef enum gc_objflags_t {
    GCS_NONE = 0u<<0,
    GCS_MARK = 1u<<0,
    GCS_ROOT = 1u<<1,
    GCS_LEAF = 1u<<2
} gc_objflags_t;
typedef size_t gchash_t;
typedef struct NEO_ALIGN(8) gc_fatptr_t { /* Fat object pointer. */
    void *ptr;
    gc_objflags_t flags : 8;
    size_t size;
    gchash_t hash;
    void (*dtor)(void *ptr); /* Destructor or NULL. */
#if NEO_DBG
    uint64_t usr; /* User data. */
#endif
} gc_fatptr_t;
neo_static_assert(sizeof(gc_fatptr_t) == 40+(sizeof((struct gc_fatptr_t){}.usr)*NEO_DBG));
neo_static_assert(sizeof(gc_fatptr_t) % 8 == 0);
neo_static_assert(sizeof(void *) == 8);
neo_static_assert(sizeof(void *)>>3 == 1);

/* Thread-local GC context. */
typedef struct gc_context_t {
    const void *stktop; /* Stack start. */
    const void *stkbot /* Stack bottom. */;
    bool is_paused;
    uintptr_t minptr;
    uintptr_t maxptr;
    ptrdiff_t delta;
    gc_fatptr_t *items; /* Root set. */
    gc_fatptr_t *frees;
    size_t nitems;
    size_t mitems;
    size_t nslots;
    size_t nfrees;
    double loadfactor;
    double sweepfactor;
} gc_context_t;

extern NEO_EXPORT void gc_init(gc_context_t *self, const void *stk_top, const void *stk_bot);
extern NEO_EXPORT void gc_pause(gc_context_t *self);
extern NEO_EXPORT void gc_resume(gc_context_t *self);
extern NEO_EXPORT void gc_collect(gc_context_t *self);
extern NEO_EXPORT void gc_free(gc_context_t *self);
extern NEO_EXPORT gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, void *p);
extern NEO_EXPORT void *gc_vmalloc(gc_context_t *self, size_t size, void (*dtor)(void *));
extern NEO_EXPORT void *gc_vmrealloc(gc_context_t *self, void *blk, size_t size);
extern NEO_EXPORT void gc_vmfree(gc_context_t *self, void **blk);
extern NEO_EXPORT gc_objflags_t gc_fatptr_get_flags(gc_context_t *self, void *p);
extern NEO_EXPORT void gc_fatptr_set_flags(gc_context_t *self, void *p, gc_objflags_t flags);
extern NEO_EXPORT void (*gc_fatptr_get_dtor(gc_context_t *self, void *p))(void *ptr);
extern NEO_EXPORT void gc_fatptr_set_dtor(gc_context_t *self, void *p, void (*dtor)(void *));
extern NEO_EXPORT size_t gc_fatptr_get_size(gc_context_t *self, void *p);

#ifdef __cplusplus
}
#endif

#endif
