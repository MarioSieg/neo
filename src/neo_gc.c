/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_gc.h"

static NEO_AINLINE size_t probe_dist(gc_context_t* self, size_t i, size_t h) {
    neo_asd(self);
    int64_t v = (int64_t)i-((int64_t)h-1);
    if (v < 0) { v = (int64_t)self->nslots + v; }
    return (size_t)v;
}

gc_fatptr_t *gc_resolve_ptr(gc_context_t *self, void *ptr) {
    neo_asd(self);
    size_t i, j, h;
    i = gc_hash(ptr) % self->nslots; j = 0;
    for (;;) {
        h = self->items[i].hash;
        if (h == 0 || j > probe_dist(self, i, h)) { return NULL; }
        if (self->items[i].ptr == ptr) { return &self->items[i]; }
        i = (i+1) % self->nslots; ++j;
    }
}

static void attach_ptr(gc_context_t *self, void *ptr, size_t size, gc_flags_t flags, void(*dtor)(void *)) {
    neo_asd(self);
    gc_fatptr_t item, tmp;
    size_t h, p, i, j;
    i = gc_hash(ptr) % self->nslots; j = 0;
    item.ptr = ptr;
    item.flags = flags;
    item.size = size;
    item.hash = i+1;
    item.dtor = dtor;
    for (;;) {
        h = self->items[i].hash;
        if (h == 0) { self->items[i] = item; return; }
        if (self->items[i].ptr == item.ptr) { return; }
        p = probe_dist(self, i, h);
        if (j >= p) {
            tmp = self->items[i];
            self->items[i] = item;
            item = tmp;
            j = p;
        }
        i = (i+1) % self->nslots; ++j;
    }
}

static void detach_ptr(gc_context_t *self, void *ptr) {
    neo_asd(self);
    size_t i, j, h, nj, nh;
    if (neo_unlikely(self->nitems == 0)) { return; }
    for (i = 0; i < self->nfrees; ++i) {
        if (self->frees[i].ptr == ptr) { self->frees[i].ptr = NULL; }
    }
    i = gc_hash(ptr) % self->nslots; j = 0;
    for (;;) {
        h = self->items[i].hash;
        if (h == 0 || j > probe_dist(self, i, h)) { return; }
        if (self->items[i].ptr == ptr) {
            memset(&self->items[i], 0, sizeof(gc_fatptr_t));
            j = i;
            for (;;) {
                nj = (j+1) % self->nslots;
                nh = self->items[nj].hash;
                if (nh != 0 && probe_dist(self, nj, nh) > 0) {
                    memcpy(&self->items[ j], &self->items[nj], sizeof(gc_fatptr_t));
                    memset(&self->items[nj], 0, sizeof(gc_fatptr_t));
                    j = nj;
                } else {
                    break;
                }
            }
            self->nitems--;
            return;
        }
        i = (i+1) % self->nslots; ++j;
    }
}

static const uint32_t prime_lut[] = {
    0,       1,       5,       11,
    23,      53,      101,     197,
    389,     683,     1259,    2417,
    4733,    9371,    18617,   37097,
    74093,   148073,  296099,  592019,
    1100009, 2200013, 4400021, 8800019
};

static size_t gc_ideal_size(const gc_context_t* self, size_t size) {
    neo_asd(self);
    size_t i, last;
    size = (size_t)((double)(size+1) / self->loadfactor);
    for (i = 0; i < sizeof(prime_lut) / sizeof(*prime_lut); ++i) {
        if (prime_lut[i] >= size) { return prime_lut[i]; }
    }
    last = prime_lut[sizeof(prime_lut) / sizeof(*prime_lut) - 1];
    for (i = 0;; ++i) {
        if (last * i >= size) { return last * i; }
    }
}

static int gc_rehash(gc_context_t* self, size_t new_size) {
    neo_asd(self);
    gc_fatptr_t *old_items = self->items;
    size_t old_size = self->nslots;
    self->nslots = new_size;
    self->items = calloc(self->nslots, sizeof(gc_fatptr_t));
    if (self->items == NULL) {
        self->nslots = old_size;
        self->items = old_items;
        return 0;
    }
    for (size_t i = 0; i < old_size; ++i) {
        if (old_items[i].hash != 0) {
            attach_ptr(self,
                       old_items[i].ptr, old_items[i].size,
                       old_items[i].flags, old_items[i].dtor);
        }
    }
    free(old_items);
    return 1;
}

static int gc_resize_more(gc_context_t *self) {
    neo_asd(self);
    size_t new_size = gc_ideal_size(self, self->nitems);
    size_t old_size = self->nslots;
    return (new_size > old_size) ? gc_rehash(self, new_size) : 1;
}

static int gc_resize_less(gc_context_t *self) {
    neo_asd(self);
    size_t new_size = gc_ideal_size(self, self->nitems);
    size_t old_size = self->nslots;
    return (new_size < old_size) ? gc_rehash(self, new_size) : 1;
}

static void gc_mark_ptr(gc_context_t *self, void *ptr) {
    neo_asd(self);
    size_t i, j, h;
    if ((uintptr_t)ptr < self->minptr
        || (uintptr_t)ptr > self->maxptr) { return; }
    i = gc_hash(ptr) % self->nslots; j = 0;
    for (;;) {
        h = self->items[i].hash;
        if (h == 0 || j > probe_dist(self, i, h)) { return; }
        if (ptr == self->items[i].ptr) {
            if (self->items[i].flags & GCF_MARK) { return; }
            self->items[i].flags |= GCF_MARK;
            if (self->items[i].flags & GCF_LEAF) { return; }
            for (size_t k = 0; k < self->items[i].size / sizeof(void*); ++k) {
                gc_mark_ptr(self, ((void**)self->items[i].ptr)[k]);
            }
            return;
        }
        i = (i+1) % self->nslots; ++j;
    }

}

static void gc_mark_stack(gc_context_t *self) {
    neo_asd(self);
    void *top = self->stk_top;
    void *bot = self->stk_bot;
    if (neo_unlikely(bot == top)) { return; }
    if (bot < top) {
        for (void *p = top; p >= bot; p = ((uint8_t *)p)-sizeof(void *)) {
            gc_mark_ptr(self, *((void **)p));
        }
    } else if (bot > top) {
        for (void *p = top; p <= bot; p = ((uint8_t *)p)+sizeof(void *)) {
            gc_mark_ptr(self, *((void **)p));
        }
    }
}

static void gc_mark(gc_context_t *self) {
    neo_asd(self);
    if (self->nitems == 0) { return; }
    for (size_t i = 0; i < self->nslots; ++i) {
        if (self->items[i].hash == 0) { continue; }
        if (self->items[i].flags & GCF_MARK) { continue; }
        if (self->items[i].flags & GCF_ROOT) {
            self->items[i].flags |= GCF_MARK;
            if (self->items[i].flags & GCF_LEAF) { continue; }
            for (size_t k = 0; k < self->items[i].size / sizeof(void*); ++k) {
                gc_mark_ptr(self, ((void**)self->items[i].ptr)[k]);
            }
            continue;
        }
    }
    gc_mark_stack(self);
}

void gc_sweep(gc_context_t *self) {
    neo_asd(self);
    size_t i, j, k, nj, nh;

    if (self->nitems == 0) { return; }

    self->nfrees = 0;
    for (i = 0; i < self->nslots; ++i) {
        if (self->items[i].hash == 0) { continue; }
        if (self->items[i].flags & GCF_MARK) { continue; }
        if (self->items[i].flags & GCF_ROOT) { continue; }
        self->nfrees++;
    }

    self->frees = realloc(self->frees, sizeof(gc_fatptr_t) * self->nfrees);
    if (self->frees == NULL) { return; }

    i = 0; k = 0;
    while (i < self->nslots) {
        if (self->items[i].hash == 0) { ++i; continue; }
        if (self->items[i].flags & GCF_MARK) { ++i; continue; }
        if (self->items[i].flags & GCF_ROOT) { ++i; continue; }

        self->frees[k] = self->items[i]; ++k;
        memset(&self->items[i], 0, sizeof(gc_fatptr_t));

        j = i;
        for (;;) {
            nj = (j+1) % self->nslots;
            nh = self->items[nj].hash;
            if (nh != 0 && probe_dist(self, nj, nh) > 0) {
                memcpy(&self->items[ j], &self->items[nj], sizeof(gc_fatptr_t));
                memset(&self->items[nj], 0, sizeof(gc_fatptr_t));
                j = nj;
            } else {
                break;
            }
        }
        self->nitems--;
    }

    for (i = 0; i < self->nslots; ++i) {
        if (self->items[i].hash == 0) { continue; }
        if (self->items[i].flags & GCF_MARK) {
            self->items[i].flags &= ~GCF_MARK&255;
        }
    }

    gc_resize_less(self);

    self->mitems = self->nitems + (size_t)((double)self->nitems * self->sweepfactor) + 1;

    for (i = 0; i < self->nfrees; ++i) {
        if (self->frees[i].ptr) {
            if (self->frees[i].dtor) { self->frees[i].dtor(self->frees[i].ptr); }
            free(self->frees[i].ptr);
        }
    }

    free(self->frees);
    self->frees = NULL;
    self->nfrees = 0;

}

void gc_init(gc_context_t *self, void *stk_top, void *stk_bot) {
    neo_asd(self);
    memset(self, 0, sizeof(*self));
    self->stk_top = stk_top;
    self->stk_bot = stk_bot;
    self->items = NULL;
    self->frees = NULL;
    self->minptr = UINTPTR_MAX;
    self->loadfactor = GC_LOADFACTOR;
    self->sweepfactor = GC_SWEEPFACTOR;
}

void gc_free(gc_context_t *self) {
    neo_asd(self);
    gc_sweep(self);
    free(self->items);
    free(self->frees);
}

void gc_pause(gc_context_t *self) {
    neo_asd(self);
    self->paused = 1;
}

void gc_resume(gc_context_t *self) {
    neo_asd(self);
    self->paused = 0;
}

void gc_collect(gc_context_t *self) {
    neo_asd(self);
    gc_mark(self);
    gc_sweep(self);
}

static void *gc_add(gc_context_t *self, void *ptr, size_t size, gc_flags_t flags, void(*dtor)(void*)) {
    neo_asd(self);
    self->nitems++;
    self->maxptr = (uintptr_t)ptr + size > self->maxptr ? (uintptr_t)ptr + size : self->maxptr;
    self->minptr = (uintptr_t)ptr < self->minptr ? (uintptr_t)ptr : self->minptr;
    if (gc_resize_more(self)) {
        if (!self->paused && self->nitems > self->mitems) {
            gc_collect(self);
        }
        attach_ptr(self, ptr, size, flags, dtor);
        return ptr;
    } else {
        self->nitems--;
        free(ptr);
        return NULL;
    }
}

static NEO_UNUSED void gc_rem(gc_context_t *self, void *ptr) {
    neo_asd(self);
    detach_ptr(self, ptr);
    gc_resize_less(self);
    self->mitems = self->nitems + self->nitems / 2 + 1;
}

void *gc_alloc(gc_context_t *self, size_t size, void(*dtor)(void *), bool is_root) {
    neo_asd(self);
    void *ptr = neo_memalloc(NULL, size);
    memset(ptr, 0, size);
    return gc_add(self, ptr, size, is_root ? GCF_ROOT : GCF_NONE, dtor);
}

void gc_set_dtor(gc_context_t *self, void *ptr, void(*dtor)(void*)) {
    neo_asd(self);
    gc_fatptr_t *p  = gc_resolve_ptr(self, ptr);
    if (p) { p->dtor = dtor; }
}

void gc_set_flags(gc_context_t *self, void *ptr, gc_flags_t flags) {
    neo_asd(self);
    gc_fatptr_t *p = gc_resolve_ptr(self, ptr);
    if (p) { p->flags = flags; }
}

gc_flags_t gc_get_flags(gc_context_t *self, void *ptr) {
    neo_asd(self);
    const gc_fatptr_t *p  = gc_resolve_ptr(self, ptr);
    return p ? p->flags : GCF_NONE;
}

void(*gc_get_dtor(gc_context_t *self, void *ptr))(void*) {
    neo_asd(self);
    const gc_fatptr_t *p  = gc_resolve_ptr(self, ptr);
    return p ? p->dtor : NULL;
}

size_t gc_get_size(gc_context_t *self, void *ptr) {
    neo_asd(self);
    const gc_fatptr_t *p  = gc_resolve_ptr(self, ptr);
    return p ? p->size : 0;
}

