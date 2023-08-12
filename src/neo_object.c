/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_object.h"

bool record_eq(record_t a, record_t b, rtag_t tag) {
    switch (tag) {
        case RT_INT: return a.as_int == b.as_int;
        case RT_FLOAT: return a.as_float == b.as_float; /* TODO: Use ULP-based comparison? */
        case RT_CHAR: return a.as_char == b.as_char;
        case RT_BOOL: return a.as_bool == b.as_bool;
        case RT_REF: return a.as_ref == b.as_ref;
        default: return false;
    }
}

void constpool_init(constpool_t *self, uint32_t cap) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    self->cap = cap ? cap : 1<<9;
    self->p = neo_memalloc(NULL, self->cap*sizeof(*self->p));
    self->tags = neo_memalloc(NULL, self->cap*sizeof(*self->tags));
}

cpkey_t constpool_put(constpool_t *self, rtag_t tag, record_t value) {
    neo_dassert(self);
    neo_assert(self->len <= CONSTPOOL_MAX && "constant pool overflow");
    if (self->len >= self->cap) {
        self->cap <<= 1;
        self->p = neo_memalloc(self->p, self->cap*sizeof(*self->p));
        self->tags = neo_memalloc(self->tags, self->cap*sizeof(*self->tags));
    }
    /* Linear search for existing entry. */
    for (cpkey_t i = 0; i < self->len; ++i) {
        if (self->tags[i] == tag && record_eq(self->p[i], value, tag)) {
            return i; /* Found existing entry. */
        }
    }
    self->p[self->len] = value;
    self->tags[self->len] = tag & 255;
    return self->len++;
}

bool constpool_has(const constpool_t *self, cpkey_t idx) {
    neo_dassert(self);
    return idx < self->len;
}

bool constpool_get(constpool_t *self, cpkey_t idx, record_t *value, rtag_t *tag) {
    neo_dassert(self && value && tag);
    if (neo_unlikely(!constpool_has(self, idx))) {
        return false;
    }
    *value = self->p[idx];
    *tag = (rtag_t)self->tags[idx];
    return true;
}

void constpool_free(constpool_t *self) {
    neo_dassert(self);
    neo_memalloc(self->p, 0);
    neo_memalloc(self->tags, 0);
}
