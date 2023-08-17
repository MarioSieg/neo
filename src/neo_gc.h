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
** -> A pointer points to it, inside memory allocated by gc_objalloc and friends.
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
** TODO: Shrink object header (compressed references, hash?)
** TODO: Store record directly on header if value type.
** TODO: What happends if data looks like a pointer but isn't?
** TODO: Generational GC.
** TODO: Define object layout with reference types first for faster scanning.
*/

#define GC_DBG NEO_DBG /* Eanble GC debug mode and logging. */
#define GC_LOADFACTOR 0.9 /* GC must be 90 % full before resizing. */
#define GC_SWEEPFACTOR 0.5 /* Trigger a sweep when the number of allocations exceeds 50% of the maximum capacity. */
#define GC_ALLOC_GRANULARITY 8 /* Allocation granularity. */
#define gc_bytesize_isvalid(s) ((s)>=GC_ALLOC_GRANULARITY&&(s)<=GC_ALLOC_MAX&&(((s)&(GC_ALLOC_GRANULARITY-1))==0)) /* Is the size in bytes valid? */
typedef uint32_t gc_grasize_t; /* Size of a memory allocation in granules. Each granule is 8 bytes large. So the smallest allocation in bytes is 8. */
#define GC_ALLOC_MAX (~0u) /* Max allocation granules. */
#define gc_grasize_valid(gra) ((gra)>0&&(gra<=GC_ALLOC_MAX))
#define gc_granules2bytes(s) ((size_t)(s)<<3) /* S / GC_ALLOC_GRANULARITY. Granules to bytes. */
#define gc_bytes2granules(g) ((size_t)(g)>>3) /* S * GC_ALLOC_GRANULARITY. Bytes to granules. */
#define gc_sizeof_granules(obj) (gc_bytes2granules(sizeof(obj))) /* Size of a structure in granules. */
#define gc_granule_align(p) (((p)+((GC_ALLOC_GRANULARITY)-1))&~((GC_ALLOC_GRANULARITY)-1))
neo_static_assert(gc_sizeof_granules(neo_int_t) == 1);
neo_static_assert(gc_sizeof_granules(neo_int_t) == 1);
neo_static_assert(GC_ALLOC_GRANULARITY >= sizeof(void*)
    && GC_ALLOC_GRANULARITY
    && ((GC_ALLOC_GRANULARITY)&(GC_ALLOC_GRANULARITY-1)) == 0
    && "GC_ALLOC_GRANULARITY must be a power of two and at least the size of a pointer.");
#if NEO_GC_HASH47
neo_static_assert(sizeof(uint64_t) == sizeof(uintptr_t));
static inline uint32_t gc_hash(const void *ptr) {
    uintptr_t p = (uintptr_t)ptr>>3;
    p &= ((1ull<<48)-1); /* Extract 48-bit address value. */
    p = (p>>32)^(p&((1ull<<32)-1)); /* Try to combine the high 16-bit with the low 32-bit of the 47/48-bit pointer. */
    return (uint32_t)p;
}
#else
#   define gc_hash(p) ((uintptr_t)(p)>>3)
#endif

typedef enum gc_flags_t {
    GCF_NONE = 0,
    GCF_MARK = 1<<0,
    GCF_ROOT = 1<<1,
    GCF_LEAF = 1<<2,
    GCF__MAX
} gc_flags_t;
neo_static_assert(GCF__MAX<=(1<<8)-1);
typedef struct NEO_ALIGN(8) gc_fatptr_t {
    void *ptr;
    gc_grasize_t grasize; /* Size in granules. */
    gc_flags_t flags : 8; /* Flags. */
    uint32_t oid : 24; /* Object layout ID. */
    uint32_t hash; /* Hashcode. */
    uint32_t reserved; /* Reserved. */
} gc_fatptr_t;
neo_static_assert(sizeof(gc_fatptr_t) == 24);
neo_static_assert(offsetof(gc_fatptr_t, ptr) == 0);
neo_static_assert(offsetof(gc_fatptr_t, grasize) == 8);
#if NEO_COM_GCC ^ NEO_COM_CLANG
neo_static_assert(__alignof__(gc_fatptr_t) == 8);
#endif

/* Per-thread GC context. */
typedef struct gc_context_t {
    const void *stktop; /* Top of the VM stack. (VM stack grows upwards so: top > bot.) */
    const void *stkbot; /* Bottom of the VM stack. (VM stack grows upwards so: top > bot.) */
    uintptr_t bndmin; /* Minimum pointer value of memory bounds. */
    uintptr_t bndmax; /* Maximum pointer value of memory bounds. */
    gc_fatptr_t *trackedallocs; /* List of tracked allocated objects. */
    size_t alloc_len; /* Allocated length of <trackedallocs>. */
    gc_fatptr_t *freelist; /* Contains temporary freed objects. */
    size_t free_len;
    size_t slots; /* Number of slots in the hashtable. */
    size_t threshold; /* Threshold value for triggering a garbage collection. */
    double loadfactor; /* Load-factor for triggering a resize. E.g., 0.75 means 75 % load of the table. */
    double sweepfactor; /* Sweep-factor for triggering a sweep. */
    volatile bool is_paused; /* Is the GC paused? */
    void (*dtor_hook)(void *); /* Destructor callback hook. */
} gc_context_t;

extern NEO_EXPORT void gc_init(gc_context_t *self, const void *stk_top, const void *stk_bot);
extern NEO_EXPORT void gc_free(gc_context_t *self);
extern NEO_EXPORT void gc_pause(gc_context_t *self);
extern NEO_EXPORT void gc_resume(gc_context_t *self);
extern NEO_EXPORT NEO_HOTPROC void gc_collect(gc_context_t *self);
extern NEO_EXPORT gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, const void *ptr);
extern NEO_EXPORT NEO_HOTPROC void *gc_objalloc(gc_context_t *self, gc_grasize_t size, gc_flags_t flags);
extern NEO_EXPORT void gc_objfree(gc_context_t *self, void *ptr);
extern NEO_EXPORT void gc_set_flags(gc_context_t *self, void *ptr, gc_flags_t flags);
extern NEO_EXPORT gc_flags_t gc_get_flags(gc_context_t *self, void *ptr);
extern NEO_EXPORT gc_grasize_t gc_get_size(gc_context_t *self, void *ptr);

#ifdef __cplusplus
}
#endif

#endif
