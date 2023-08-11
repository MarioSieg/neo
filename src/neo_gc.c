/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_gc.h"

#if GC_DBG
#   define gctrace(msg, ...) neo_info("[gc] " msg, ## __VA_ARGS__)
#else
#   define gctrace(msg, ...)
#endif

static inline size_t probe_dist(const gc_context_t* self, size_t i, size_t h) {
    neo_dassert(self);
    int64_t v = (int64_t)i-((int64_t)h-1);
    if (v < 0) { v = (int64_t)self->slots+v; }
    return (size_t)v;
}

gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, const void *ptr) {
    neo_dassert(self);
    size_t i, j, h;
    i = gc_hash(ptr) % self->slots; j = 0;
    for (;;) {
        h = self->trackedallocs[i].hash;
        if (neo_unlikely(h == 0 || j > probe_dist(self, i, h))) { return NULL; }
        if (self->trackedallocs[i].ptr == ptr) { return self->trackedallocs+i; }
        i = (i+1) % self->slots; ++j;
    }
}

static void attach_ptr(gc_context_t *self, void *ptr, gc_grasize_t size, gc_flags_t flags) {
    neo_dassert(self);
    gc_fatptr_t item, tmp;
    size_t h, p, i, j;
    i = gc_hash(ptr) % self->slots; j = 0;
    item.ptr = ptr;
    item.flags = flags;
    item.grasize = size;
    item.hash = (uint32_t)(i+1);
    for (;;) {
        h = self->trackedallocs[i].hash;
        if (h == 0) { self->trackedallocs[i] = item; return; }
        if (self->trackedallocs[i].ptr == item.ptr) { return; }
        p = probe_dist(self, i, h);
        if (j >= p) {
            tmp = self->trackedallocs[i];
            self->trackedallocs[i] = item;
            item = tmp;
            j = p;
        }
        i = (i+1) % self->slots; ++j;
    }
}

static void detach_ptr(gc_context_t *self, void *ptr) {
    neo_dassert(self);
    size_t i, j, h, nj, nh;
    if (neo_unlikely(self->alloc_len == 0)) { return; }
    for (i = 0; i < self->free_len; ++i) {
        if (self->freelist[i].ptr == ptr) { self->freelist[i].ptr = NULL; }
    }
    i = gc_hash(ptr) % self->slots; j = 0;
    for (;;) {
        h = self->trackedallocs[i].hash;
        if (h == 0 || j > probe_dist(self, i, h)) { return; }
        if (self->trackedallocs[i].ptr == ptr) {
            memset(self->trackedallocs+i, 0, sizeof(*self->trackedallocs));
            j = i;
            for (;;) {
                nj = (j+1) % self->slots;
                nh = self->trackedallocs[nj].hash;
                if (nh && probe_dist(self, nj, nh) > 0) {
                    memcpy(self->trackedallocs+j, self->trackedallocs+nj, sizeof(*self->trackedallocs));
                    memset(self->trackedallocs+nj, 0, sizeof(*self->trackedallocs));
                    j = nj;
                } else {
                    break;
                }
            }
            self->alloc_len--;
            return;
        }
        i = (i+1) % self->slots; ++j;
    }
}

static const uint32_t prime_lut[] = {
    0x0,0x1,0x5,0xb,
    0x17,0x35,0x65,0xc5,
    0x185, 0x2ab, 0x4eb, 0x971, 0x127d,
    0x249b,0x48b9,0x90e9,0x1216d, 0x24269,
    0x484a3, 0x90893, 0x10c8e9, 0x2191cd, 0x432395,0x864713
};

static size_t gc_ideal_size(const gc_context_t* self, size_t size) {
    neo_dassert(self);
    size = (size_t)((double)(size+1)/self->loadfactor);
    for (size_t i = 0; i < sizeof(prime_lut)/sizeof(*prime_lut); ++i) {
        if (prime_lut[i] >= size) { return prime_lut[i]; }
    }
    size_t last = prime_lut[sizeof(prime_lut)/sizeof(*prime_lut)-1];
    for (size_t i = 0; ; ++i) {
        if (last*i >= size) { return last*i; }
    }
}

static void rehash_alloc_map(gc_context_t* self, size_t new_size) {
    neo_dassert(self);
    gc_fatptr_t *old_items = self->trackedallocs;
    size_t old_size = self->slots;
    self->slots = new_size;
    self->trackedallocs = neo_memalloc(NULL, self->slots*sizeof(gc_fatptr_t));
    for (size_t i = 0; i < old_size; ++i) {
        if (neo_likely(old_items[i].hash)) {
            attach_ptr(self, old_items[i].ptr, old_items[i].grasize, old_items[i].flags);
        }
    }
    neo_memalloc(old_items, 0);
}

static void grow_alloc_map(gc_context_t *self) {
    neo_dassert(self);
    size_t new_size = gc_ideal_size(self, self->alloc_len);
    size_t old_size = self->slots;
    if (new_size > old_size) { rehash_alloc_map(self, new_size); }
}

static void shrink_alloc_map(gc_context_t *self) {
    neo_dassert(self);
    size_t new_size = gc_ideal_size(self, self->alloc_len);
    size_t old_size = self->slots;
    if (new_size < old_size) { rehash_alloc_map(self, new_size); }
}

static void gc_mark_ptr(gc_context_t *self, const void *ptr) {
    neo_dassert(self);
    size_t i, j, h;
    if (neo_unlikely((uintptr_t)ptr < self->bndmin || (uintptr_t)ptr > self->bndmax)) { return; } /* Out of bounds. */
    i = gc_hash(ptr) % self->slots; j = 0;
    for (;;) {
        h = self->trackedallocs[i].hash;
        if (h == 0 || j > probe_dist(self, i, h)) { return; }
        if (ptr == self->trackedallocs[i].ptr) {
            if (self->trackedallocs[i].flags&GCF_MARK) { return; }
            self->trackedallocs[i].flags|=GCF_MARK;
            if (self->trackedallocs[i].flags&GCF_LEAF) { return; }
            const void **top = (const void **)self->trackedallocs[i].ptr;
            const void **end = top+gc_granules2bytes(self->trackedallocs[i].grasize)/sizeof(void*);
            while (top < end) { gc_mark_ptr(self, *top++); }
            return;
        }
        i = (i+1) % self->slots; ++j;
    }

}

static void gc_mark_stack(gc_context_t *self) {
    neo_dassert(self);
    if (neo_unlikely(self->stktop == self->stkbot)) { return; }
    const void **top = (const void **)self->stktop;
    const void **bot = (const void **)self->stkbot;
    if (bot < top) { while (top >= bot) { gc_mark_ptr(self, *top--); } }
    else { while (top <= bot) { gc_mark_ptr(self, *top++); } }
}

static void gc_mark(gc_context_t *self) {
    neo_dassert(self);
    if (neo_unlikely(!self->alloc_len)) { return; }
    for (size_t i = 0; i < self->slots; ++i) {
        if (!self->trackedallocs[i].hash || (self->trackedallocs[i].flags&GCF_MARK)) { continue; }
        if (self->trackedallocs[i].flags&GCF_ROOT) {
            self->trackedallocs[i].flags|=GCF_MARK;
            if (self->trackedallocs[i].flags&GCF_LEAF) { continue; }
            const void **top = (const void **)self->trackedallocs[i].ptr;
            const void **end = top+gc_granules2bytes(self->trackedallocs[i].grasize)/sizeof(void*);
            while (top < end) { gc_mark_ptr(self, *top++); }
            continue;
        }
    }
    gc_mark_stack(self);
}

void gc_sweep(gc_context_t *self) {
    neo_dassert(self);
    size_t i, j, k, nj, nh;
    if (neo_unlikely(!self->alloc_len)) { return; }
    self->free_len = 0;
    for (i = 0; i < self->slots; ++i) {
        if (!self->trackedallocs[i].hash || (self->trackedallocs[i].flags&(GCF_MARK|GCF_ROOT))) { continue; }
        ++self->free_len;
    }
    self->freelist = neo_memalloc(self->freelist, sizeof(*self->freelist)*self->free_len);
    i = 0; k = 0;
    while (i < self->slots) {
        if (!self->trackedallocs[i].hash || (self->trackedallocs[i].flags&(GCF_MARK|GCF_ROOT))) { ++i; continue; }
        self->freelist[k++] = self->trackedallocs[i];
        memset(self->trackedallocs+i, 0, sizeof(*self->trackedallocs));
        j = i;
        for (;;) {
            nj = (j+1) % self->slots;
            nh = self->trackedallocs[nj].hash;
            if (nh && probe_dist(self, nj, nh) > 0) {
                memcpy(self->trackedallocs+j, self->trackedallocs+nj, sizeof(*self->trackedallocs));
                memset(self->trackedallocs+nj, 0, sizeof(*self->trackedallocs));
                j = nj;
            } else {
                break;
            }
        }
        ++self->alloc_len;
    }
    for (i = 0; i < self->slots; ++i) {
        if (self->trackedallocs[i].hash == 0) { continue; }
        if (self->trackedallocs[i].flags&GCF_MARK) {
            self->trackedallocs[i].flags&=~GCF_MARK&255;
        }
    }
    shrink_alloc_map(self);
    self->threshold = self->alloc_len+(size_t)((double)self->alloc_len*self->sweepfactor)+1;
    for (i = 0; i < self->free_len; ++i) {
        if (self->freelist[i].ptr) {
            if (self->dtor_hook) { (*self->dtor_hook)(self->freelist[i].ptr); }
            void *volatile p = self->freelist[i].ptr;
            neo_memalloc(p, 0); /* Free individual allocation. */
        }
    }
    neo_memalloc(self->freelist, 0);
    self->freelist = NULL;
    self->free_len = 0;
}

void gc_init(gc_context_t *self, const void *stk_top, const void *stk_bot) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    self->stktop = stk_top;
    self->stkbot = stk_bot;
    self->trackedallocs = NULL;
    self->freelist = NULL;
    self->bndmin = UINTPTR_MAX;
    self->loadfactor = GC_LOADFACTOR;
    self->sweepfactor = GC_SWEEPFACTOR;
    gctrace("initialized gc with stack bounds: [%p, %p], delta: %zub", stk_top, stk_bot, (size_t)llabs((intptr_t)stk_bot-(intptr_t)stk_top));
}

void gc_free(gc_context_t *self) {
    neo_dassert(self);
    gc_sweep(self);
    for (size_t i = 0; i < self->slots; ++i) { /* Free all roots. */
        if (self->trackedallocs[i].ptr && self->trackedallocs[i].flags&GCF_ROOT) {
            neo_warn("root memory allocation still alive: %p, index: %zu, size: %zub" PRIu32, self->trackedallocs[i].ptr, i, gc_bytes2granules(self->trackedallocs[i].grasize));
            gc_objfree(self, self->trackedallocs[i].ptr);
        }
    }
    neo_memalloc(self->trackedallocs, 0);
    neo_memalloc(self->freelist, 0);
    memset(self, 0, sizeof(*self));
    gctrace("offline");
}

void gc_pause(gc_context_t *self) {
    neo_dassert(self);
    self->is_paused = true;
}

void gc_resume(gc_context_t *self) {
    neo_dassert(self);
    self->is_paused = false;
}

NEO_HOTPROC void gc_collect(gc_context_t *self) {
    neo_dassert(self);
    gctrace("collecting garbage...");
    gc_mark(self);
    gc_sweep(self);
}

static void *attach_objptr(gc_context_t *self, void *ptr, gc_grasize_t size, gc_flags_t flags) {
    neo_dassert(self);
    ++self->alloc_len;
    self->bndmax = (uintptr_t)ptr+gc_granules2bytes(size) > self->bndmax ? (uintptr_t)ptr+gc_granules2bytes(size) : self->bndmax;
    self->bndmin = (uintptr_t)ptr < self->bndmin ? (uintptr_t)ptr : self->bndmin;
    grow_alloc_map(self);
    if (!self->is_paused && self->alloc_len > self->threshold) {
        gctrace("allocation threshold reached, triggered collection");
        gc_collect(self);
    }
    attach_ptr(self, ptr, size, flags);
    gctrace("allocated %zu b / (%"PRIu32" gra) / %f MiB at %p, flags: %x", gc_granules2bytes(size), size, (double)gc_granules2bytes(size)/pow(1024.0, 2.0), ptr, flags);
    return ptr;
}

static void detach_objptr(gc_context_t *self, void *ptr) {
    neo_dassert(self);
    detach_ptr(self, ptr);
    shrink_alloc_map(self);
    self->threshold = 1+self->alloc_len+(self->alloc_len>>1);
    gctrace("deallocated %p", ptr);
}

NEO_HOTPROC void *gc_objalloc(gc_context_t *self, gc_grasize_t size, gc_flags_t flags) {
    neo_dassert(self);
    neo_assert(gc_grasize_valid(size) && "invalid gc allocation granule size, must be > 0 and <= 2^32-1");
    void *ptr = neo_memalloc(NULL, gc_granules2bytes(size));
    memset(ptr, 0, gc_granules2bytes(size)); /* Zero memory and warmup pages. */
    return attach_objptr(self, ptr, size, flags);
}

void gc_objfree(gc_context_t *self, void *ptr) {
    neo_dassert(self);
    if (neo_unlikely(!ptr)) { return; }
    const gc_fatptr_t *p = gc_resolve_ptr(self, ptr);
    if (p) {
        if (self->dtor_hook) { (*self->dtor_hook)(ptr); }
        neo_memalloc(ptr, 0); /* Free individual allocation. */
        detach_objptr(self, ptr);
    }
}

void gc_set_flags(gc_context_t *self, void *ptr, gc_flags_t flags) {
    neo_dassert(self);
    gc_fatptr_t *p = gc_resolve_ptr(self, ptr);
    if (p) { p->flags = flags; }
}

gc_flags_t gc_get_flags(gc_context_t *self, void *ptr) {
    neo_dassert(self);
    const gc_fatptr_t *p = gc_resolve_ptr(self, ptr);
    return p ? p->flags : GCF_NONE;
}

gc_grasize_t gc_get_size(gc_context_t *self, void *ptr) {
    neo_dassert(self);
    const gc_fatptr_t *p = gc_resolve_ptr(self, ptr);
    return p ? p->grasize : 0;
}
