/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_vm.h"

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
    neo_asd(self);
    memset(self, 0, sizeof(*self));
    self->cap = cap ? cap : 1<<9;
    self->p = neo_memalloc(NULL, self->cap*sizeof(*self->p));
    self->tags = neo_memalloc(NULL, self->cap*sizeof(*self->tags));
}

cpkey_t constpool_put(constpool_t *self, rtag_t tag, record_t value) {
    neo_asd(self);
    neo_as(self->len <= CONSTPOOL_MAX && "constant pool overflow");
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
    neo_asd(self);
    return idx < self->len;
}

bool constpool_get(constpool_t *self, cpkey_t idx, record_t *value, rtag_t *tag) {
    neo_asd(self && value && tag);
    if (neo_unlikely(!constpool_has(self, idx))) {
        return false;
    }
    *value = self->p[idx];
    *tag = (rtag_t)self->tags[idx];
    return true;
}

void constpool_free(constpool_t *self) {
    neo_asd(self);
    neo_memalloc(self->p, 0);
    neo_memalloc(self->tags, 0);
}

bool vm_validate(const vmisolate_t *isolate, const bytecode_t *bcode) {
    neo_as(isolate && bcode);
    const bci_instr_t *code = bcode->p;
    size_t len = bcode->len;
    if (neo_unlikely(!code || !len)) {
        neo_error("invalid code pointer or length: %zu", len);
        return false;
    }
    if (neo_unlikely(bci_unpackopc(code[0]) != OPC_NOP)) {
        neo_error("first instruction must be NOP, but instead is: %s", opc_mnemonic[bci_unpackopc(code[0])]);
        return false;
    }
    if (neo_unlikely(bci_unpackopc(code[len-1]) != OPC_HLT)) {
        neo_error("last instruction must be HLT, but instead is: %s", opc_mnemonic[bci_unpackopc(code[len-1])]);
        return false;
    }
    for (size_t i = 0; i < len; ++i) { /* Validate the encoding of all instructions. */
        if (neo_unlikely(!bci_validate_instr(code[i]))) {
            neo_error("invalid instruction at index: %zu", i);
            return false;
        }
        opcode_t opc = bci_unpackopc(code[i]);
        if (neo_unlikely(opc == OPC_LDC)) { /* Specific instruction validation. */
            uint32_t umm24 = bci_mod1unpack_umm24(code[i]);
            if (neo_unlikely(umm24 >= isolate->constpool.len)) {
                neo_error("invalid constant pool index: %" PRIi32, umm24);
                return false;
            }
        }
    }
    return true;
}
