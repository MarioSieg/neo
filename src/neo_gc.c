/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_gc.h"

static const uint32_t prime_lut[24] = {
    0,       1,       5,       11,
    23,      53,      101,     197,
    389,     683,     1259,    2417,
    4733,    9371,    18617,   37097,
    74093,   148073,  296099,  592019,
    1100009, 2200013, 4400021, 8800019
};

#define hash_ptr(p) ((uintptr_t)(p)>>3)

static NEO_AINLINE size_t probe_dist(const gc_context_t *self, size_t i, gchash_t h) { /* Calculate probing distance on collision. */
    neo_asd(self);
    int64_t delta = (int64_t)i-(int64_t)h-1;
    if (delta < 0) { delta += (int64_t)self->nslots; }
    return (size_t)delta;
}

static size_t compute_ideal_size(const gc_context_t *self, size_t size) {
    neo_asd(self);
    size = (size_t)((double)(size+1)/self->loadfactor);
    for (size_t i = 0; i < sizeof(prime_lut)/sizeof(*prime_lut); ++i) {
        if (prime_lut[i] >= size) { return prime_lut[i]; }
    }
    size_t last = prime_lut[sizeof(prime_lut)/sizeof(*prime_lut)-1];
    for (size_t i = 0; ; ++i) {
        size_t k = last*i;
        if (k >= size) { return k; }
    }
}

static void attach_ptr(gc_context_t *self, void *p, size_t size, gc_objflags_t flags, void (*dtor)(void *ptr));
static void rehash(gc_context_t *self, size_t new_size) {
    gc_fatptr_t *prev = self->items;
    size_t nprev = self->nslots;
    self->nslots = new_size;
    self->items = neo_memalloc(NULL, sizeof(*self->items)*new_size);
    memset(self->items, 0, sizeof(*self->items)*new_size);
    for (size_t i = 0; i < nprev; ++i) {
        if (prev[i].hash) {
            attach_ptr(self, prev[i].ptr, prev[i].size, prev[i].flags, prev[i].dtor);
        }
    }
    neo_memalloc(prev, 0);
}

static void resize_grow(gc_context_t *self) {
    neo_asd(self);
    size_t new_size = compute_ideal_size(self, self->nitems);
    if (new_size > self->nslots) { rehash(self, new_size); }
}

static void resize_shrink(gc_context_t *self) {
    neo_asd(self);
    size_t new_size = compute_ideal_size(self, self->nitems);
    if (new_size < self->nslots) { rehash(self, new_size); }
}

static void attach_ptr(gc_context_t *self, void *p, size_t size, gc_objflags_t flags, void (*dtor)(void *ptr)) {
    neo_asd(self);
    gchash_t i = hash_ptr(p) % self->nslots;
    gc_fatptr_t fat_ptr;
    fat_ptr.ptr = p;
    fat_ptr.flags = flags;
    fat_ptr.size = size;
    fat_ptr.hash = i+1;
    fat_ptr.dtor = dtor;
    for (size_t j = 0; ; ++j) {
        gchash_t h = self->items[i].hash;
        if (!h) { self->items[i] = fat_ptr; return; }
        else if (self->items[i].ptr == p) { return; }
        size_t pr = probe_dist(self, i, h);
        if (j >= pr) {
            gc_fatptr_t tmp = self->items[i];
            self->items[i] = fat_ptr;
            fat_ptr = tmp;
            j = pr;
        }
        i = (i+1) % self->nslots;
    }
}

static void detach_ptr(gc_context_t *self, void *p) {
    neo_asd(self);
    if (neo_unlikely(!p || !self->nitems)) { return; }
    for (size_t i = 0; i < self->nfrees; ++i) {
        if (self->frees[i].ptr == p) { self->frees[i].ptr = NULL; return; }
    }
    gchash_t i = hash_ptr(p) % self->nslots;
    for (size_t j = 0; ; ++j) {
        gchash_t h = self->items[i].hash;
        if (!h || j > probe_dist(self, i, h)) { return; }
        else if (self->items[i].ptr == p) {
            memset(self->items+i, 0, sizeof(*self->items));
            j = i;
            for (;;) {
                size_t nj = (j+1) % self->nslots;
                gchash_t nh = self->items[nj].hash;
                if (nh && probe_dist(self, nj, nh) > 0) {
                    memcpy(self->items+j, self->items+nj, sizeof(*self->items));
                    memset(self->items+nj, 0, sizeof(*self->items));
                    j = nj;
                } else {
                    break;
                }
            }
            --self->nitems;
            return;
        }
        i = (i+1) % self->nslots;
    }
}

static void mark_ptr(gc_context_t *self, const void *p) {
    neo_asd(self);
    if (neo_unlikely((uintptr_t)p < self->minptr
        || (uintptr_t)p > self->maxptr)) { return; } /* Out of bounds. */
    size_t i = hash_ptr(p) % self->nslots;
    for (size_t j = 0; ; ++j) {
        gchash_t h = self->items[i].hash;
        if (!h || j > probe_dist(self, i, h)) { return; }
        else if (self->items[i].ptr == p) {
            if (self->items[i].flags & GCS_MARK) { return; } /* Already marked. */
            self->items[i].flags |= GCS_MARK; /* Set as marked. */
            if (self->items[i].flags & GCS_LEAF) { return; } /* No following addresses to mark (LEAF node). */
            for (size_t k = 0; k < self->items[i].size>>3; ++k) { /* Mark the following linear addresses. */
                mark_ptr(self, ((void **)self->items[i].ptr)[k]);
            }
        }
        i = (i+1) % self->nslots;
    }
}

static void mark_stack(gc_context_t *self) {
    neo_asd(self);
    const void *top = self->stktop;
    const void *bot = self->stkbot;
    neo_as(top && bot && top != bot);
    if ((uintptr_t)bot < (uintptr_t)top) {
        for (const void *p = top; p >= bot; p = ((const uint8_t *)p)-sizeof(void*)) {
            mark_ptr(self, *((const void **)p));
        }
    } else if ((uintptr_t)bot > (uintptr_t)top) {
        for (const void *p = top; p <= bot; p = ((const uint8_t *)p)+sizeof(void*)) {
            mark_ptr(self, *((const void **)p));
        }
    }
}

static void mark(gc_context_t *self) {
    neo_asd(self);
    if (neo_unlikely(!self->nitems)) { return; }
    for (size_t i = 0; i < self->nitems; ++i) {
        if (neo_unlikely(!self->items[i].hash)) { continue; }
        else if (self->items[i].flags & GCS_MARK) { continue; }
        else if (self->items[i].flags & GCS_ROOT) {
            self->items[i].flags |= GCS_MARK;
            if (self->items[i].flags & GCS_LEAF) { continue; }
            for (size_t k = 0; k < self->items[i].size>>3; ++k) {
                mark_ptr(self, ((void **)self->items[i].ptr)[k]);
            }
            continue;
        }
    }
    mark_stack(self);
}

static void sweep(gc_context_t *self) {
    neo_asd(self);
    if (neo_unlikely(!self->nitems)) { return; }
    self->nfrees = 0;
    for (size_t i = 0; i < self->nslots; ++i) {
        if (!self->items[i].hash || (self->items[i].flags & (GCS_MARK|GCS_ROOT))) { continue; }
        ++self->nfrees;
    }
    self->frees = neo_memalloc(self->frees, self->nfrees*sizeof(*self->frees));
    for (size_t i = 0, k = 0; i < self->nslots; --self->nitems) {
        if (!self->items[i].hash || (self->items[i].flags & (GCS_MARK|GCS_ROOT))) { ++i; continue; }
        self->frees[k] = self->items[i];
        ++k;
        memset(self->items+i, 0, sizeof(*self->items));
        size_t j = i;
        for (;;) {
            size_t nj = (j+1) % self->nslots;
            size_t nh = self->items[nj].hash;
            if (nh && probe_dist(self, nj, nh) > 0) {
                memcpy(self->items+j, self->items+nj, sizeof(*self->items));
                memset(self->items+nj, 0, sizeof(*self->items));
                j = nj;
            } else {
                break;
            }
        }
    }
    for (size_t i = 0; i < self->nslots; ++i) {
        if (neo_unlikely(!self->items[i].hash)) { continue; }
        else if (self->items[i].flags & GCS_MARK) {
            self->items[i].flags &= ~GCS_MARK&255;
        }
    }
    resize_shrink(self);
    self->mitems = self->nitems+(size_t)((double)self->nitems*self->sweepfactor)+1;
    for (size_t i = 0; i < self->nfrees; ++i) {
        if (self->frees[i].ptr) {
            if (self->frees[i].dtor) { (*self->frees[i].dtor)(self->frees[i].ptr); } /* Invoke destructor if any. */
            neo_memalloc(self->frees[i].ptr, 0);
        }
    }
    neo_memalloc(self->frees, 0);
    self->frees = NULL;
    self->nfrees = 0;
}

static void *attach(gc_context_t *self, void *p, size_t size, gc_objflags_t flags, void (*dtor)(void *)) {
    neo_asd(self && p && size);
    ++self->nitems;
    self->maxptr = (uintptr_t)p+size > self->maxptr ? (uintptr_t)p+size : self->maxptr;
    self->minptr = (uintptr_t)p < self->minptr ? (uintptr_t)p : self->minptr;
    resize_grow(self);
    attach_ptr(self, p, size, flags, dtor);
    if (!self->is_paused && self->nitems > self->mitems) {
        gc_collect(self);
    }
    return p;
}

static void detach(gc_context_t *self, void *p) {
    neo_asd(self && p);
    detach_ptr(self, p);
    self->mitems = self->nitems+(self->nitems>>1)+1;
}

gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, void *p) {
    neo_asd(self && p);
    size_t i = hash_ptr(p) % self->nslots;
    for (size_t j = 0; ; ++j) {
        gchash_t h = self->items[i].hash;
        if (!h || j > probe_dist(self, i, h)) { return NULL; }
        else if (self->items[i].ptr == p) { return &self->items[i]; }
        i = (i+1) % self->nslots;
    }
}

void gc_init(gc_context_t *self, const void *stk_top, const void *stk_bot) {
    neo_asd(self && stk_bot && stk_top && stk_top != stk_bot);
    memset(self, 0, sizeof(*self));
    self->stktop = stk_top;
    self->stkbot = stk_bot;
    self->delta = llabs((const uint8_t *)stk_bot-(const uint8_t *)stk_top);
    self->minptr = UINTPTR_MAX;
    self->loadfactor = GC_LOADFACTOR;
    self->sweepfactor = GC_SWEEPFACTOR;
}

void gc_pause(gc_context_t *self) {
    neo_asd(self);
    self->is_paused = true;
}

void gc_resume(gc_context_t *self) {
    neo_asd(self);
    self->is_paused = false;
}

void gc_collect(gc_context_t *self) {
    neo_asd(self);
    mark(self);
    sweep(self);
}

void gc_free(gc_context_t *self) {
    neo_asd(self);
    sweep(self);
    neo_memalloc(self->items, 0);
    neo_memalloc(self->frees, 0);
}

void *gc_vmalloc(gc_context_t *self, size_t size, void (*dtor)(void *)) {
    neo_asd(self);
    neo_as(size && "zero GC allocation requested");
    void *p = neo_memalloc(NULL, size); /* Allocate memory. TODO: Thread-local heaps. */
    memset(p, 0, size); /* Zero memory and pre-warm pages. */
    return attach(self, p, size, GCS_NONE, dtor); /* Attach ptr to GC. */
}

void *gc_vmrealloc(gc_context_t *self, void *blk, size_t size) {
    neo_asd(self);
    neo_as(size && "zero GC allocation requested");
    void *qp = neo_memalloc(blk, size);
    if (neo_unlikely(!blk)) { /* NULL blk results in normal allocation. */
        attach(self, qp, size, GCS_NONE, NULL);
        return qp;
    }
    gc_fatptr_t *fptr = gc_resolve_ptr(self, qp);
    if (qp == blk) { /* No realloaction happend (block was tail-extended) - just update size. */
        fptr->size = size;
        return qp;
    } else { /* Full reallocation happened, re-insert tracking. */
        gc_objflags_t flags = fptr->flags;
        void (*dtor)(void *) = fptr->dtor;
        detach(self, blk);
        attach(self, qp, size, flags, dtor);
        return qp;
    }
}

void gc_vmfree(gc_context_t *self, void **blk) {
    neo_asd(self);
    if (neo_unlikely(!blk || !*blk)) { return; }
    gc_fatptr_t *fptr = gc_resolve_ptr(self, *blk);
    if (fptr->dtor) { (*fptr->dtor)(*blk); } /* Invoke destructor, if any. */
    neo_memalloc(*blk, 0); /* Free assoc memory. TODO: Thread-local heaps. */
    detach(self, *blk); /* Detach from GC. */
    *blk = NULL;
}

gc_objflags_t gc_fatptr_get_flags(gc_context_t *self, void *p) {
    const gc_fatptr_t *fptr = gc_resolve_ptr(self, p);
    return neo_likely(fptr) ? fptr->flags : 0;
}
void gc_fatptr_set_flags(gc_context_t *self, void *p, gc_objflags_t flags) {
    gc_fatptr_t *fptr = gc_resolve_ptr(self, p);
    if (neo_likely(fptr)) { fptr->flags = flags; }
}
void (*gc_fatptr_get_dtor(gc_context_t *self, void *p))(void *ptr) {
    const gc_fatptr_t *fptr = gc_resolve_ptr(self, p);
    return neo_likely(fptr) ? fptr->dtor : NULL;
}
void gc_fatptr_set_dtor(gc_context_t *self, void *p, void (*dtor)(void *)) {
    gc_fatptr_t *fptr = gc_resolve_ptr(self, p);
    if (neo_likely(fptr)) { fptr->dtor = dtor; }
}
size_t gc_fatptr_get_size(gc_context_t *self, void *p) {
    const gc_fatptr_t *fptr = gc_resolve_ptr(self, p);
    return neo_likely(fptr) ? fptr->size : 0;
}
