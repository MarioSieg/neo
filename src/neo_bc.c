/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_bc.h"
#include "neo_vm.h"

#define _(_1, mnemonic, _3, _4, _5) [_1] = mnemonic
const char *const opc_mnemonic[OPC__LEN] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, ops, _4, _5) [_1] = (ops&255)
const uint8_t opc_stack_ops[OPC__LEN] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, _3, rtv, _5) [_1] = (rtv&255)
const uint8_t opc_stack_rtvs[OPC__LEN] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, ops, rtv, _5) [_1] = (int8_t)(-(int8_t)(ops)+(int8_t)(rtv))
const int8_t opc_depths[OPC__LEN] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, _3, _4, imm) [_1] = imm
const uint8_t opc_imms[OPC__LEN] = {opdef(_, NEO_SEP)};
#undef _

#define _(_1, ops, _3, _4) [_1] = (ops&255)
const uint8_t syscall_stack_ops[SYSCALL__LEN] = {syscalldef(_, NEO_SEP)};
#undef _
#define _(_1, _2, rtv, _4) [_1] = (rtv&255)
const uint8_t syscall_stack_rtvs[SYSCALL__LEN] = {syscalldef(_, NEO_SEP)};
#undef _
#define _(_1, ops, rtv, _4) [_1] = (int8_t)(-(int8_t)(ops)+(int8_t)(rtv))
const int8_t syscall_depths[SYSCALL__LEN] = {syscalldef(_, NEO_SEP)};
#undef _
#define _(_1, _2, _3, mnemonic) [_1] = mnemonic
const char *const syscall_mnemonic[SYSCALL__LEN] = {syscalldef(_, NEO_SEP)};
#undef _

void bci_dump_instr(bci_instr_t instr, FILE *out, bool colored) {
    neo_dassert(out);
    uint32_t mod = bci_unpackmod(instr);
    if (neo_likely(mod == BCI_MOD1)) {
        opcode_t opc = bci_unpackopc(instr);
        const char *cc_mnemonic = colored ? NEO_CCBLUE : "";
        const char *cc_k = colored ? NEO_CCMAGENTA : "";
        const char *cc_reset = colored ? NEO_CCRESET : "";
        if (opc_imms[opc]) {
            imm24_t i = bci_mod1unpack_imm24(instr);
            fprintf(
                out,
                i > 0xffff ? "%s%s%s %s#%" PRIx32 "%s" : "%s%s%s %s#%" PRIi32 "%s",
                cc_mnemonic,
                opc_mnemonic[opc],
                cc_reset,
                cc_k,
                i,
                cc_reset
            );
        } else {
            fprintf(out, "%s%s%s", cc_mnemonic, opc_mnemonic[opc], cc_reset);
        }
    } else {
        /* NYI */
    }
}

void metaspace_init(metaspace_t *self, uint32_t cap) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    self->cap = cap ? cap : 1<<9;
    self->p = neo_memalloc(NULL, self->cap*sizeof(*self->p));
    self->tags = neo_memalloc(NULL, self->cap*sizeof(*self->tags));
}

cpkey_t metaspace_insert_kv(metaspace_t *self, rtag_t tag, record_t value) {
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

bool metaspace_contains_k(const metaspace_t *self, cpkey_t idx) {
    neo_dassert(self);
    return idx < self->len;
}

bool metaspace_get(const metaspace_t *self, cpkey_t idx, record_t *value, rtag_t *tag) {
    neo_dassert(self && value && tag);
    if (neo_unlikely(!metaspace_contains_k(self, idx))) {
        return false;
    }
    *value = self->p[idx];
    *tag = (rtag_t)self->tags[idx];
    return true;
}

void metaspace_free(metaspace_t *self) {
    neo_dassert(self);
    neo_memalloc(self->p, 0);
    neo_memalloc(self->tags, 0);
}

void bc_init(bytecode_t *self) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    self->p = neo_memalloc(NULL, (self->cap=1<<6)*sizeof(*self->p));
    *self->p = bci_comp_mod1_no_imm(OPC_NOP); /* First instruction must be NOP. */
    metaspace_init(&self->pool, 0);
}

void bc_emit(bytecode_t *self, bci_instr_t instr) {
    neo_dassert(self);
    if (self->len == self->cap) {
        self->p = neo_memalloc(self->p, (self->cap<<=1)*sizeof(*self->p));
    }
    self->p[self->len++] = instr;
}

const bci_instr_t *bc_finalize(bytecode_t *self) {
    neo_dassert(self);
    if (bci_unpackopc(self->p[self->len-1]) != OPC_HLT) {
        bc_emit(self, bci_comp_mod1_no_imm(OPC_HLT)); /* Last instruction must be HLT. */
    }
    self->p = neo_memalloc(self->p, self->len*sizeof(*self->p)); /* Shrink to fit. */
    return self->p;
}

void bc_free(bytecode_t *self) {
    neo_dassert(self);
    metaspace_free(&self->pool);
    neo_memalloc(self->p, 0);
}

static NEO_COLDPROC void bitdump(uint8_t x, FILE *f) {
    neo_dassert(f);
    for (int i = (sizeof(x)<<3)-1; i >= 0; --i) {
        fputc((x >> i) & 1 ? '1' : '0', f);
    }
}

void bc_disassemble(const bytecode_t *self, FILE *f, bool colored) {
    neo_dassert(self && f);
    for (int i = 0; i < 64; ++i) { fputc('-', f); }
    fputc('\n', f);
    fprintf(f, "NEO BYTECODE V.%d, L: %zu, S: %zub\n", self->ver, self->len, self->len*sizeof(*self->p));
    const char *cc_addr = colored ? NEO_CCYELLOW : "";
    const char *cc_opcode = colored ? NEO_CCRED : "";
    const char *cc_encoding = colored ? NEO_CCCYAN : "";
    const char *cc_comment = colored ? NEO_CCGREEN : "";
    const char *cc_mnemonic = colored ? NEO_CCBLUE : "";
    const char *cc_imm = colored ? NEO_CCMAGENTA : "";
    const char *cc_reset = colored ? NEO_CCRESET : "";
    fprintf(
        f,
        " %sADDR%s |  %sOPCODE%s  | %sENCODING%s | %sMNEMONIC%s | %sIMM%s\n",
        cc_addr,
        cc_reset,
        cc_opcode,
        cc_reset,
        cc_encoding,
        cc_reset,
        cc_mnemonic,
        cc_reset,
        cc_imm,
        cc_reset
    );
    for (size_t i = 0; i < self->len; ++i) {
        opcode_t opc = bci_unpackopc(self->p[i]);
        fprintf(f, "%s0x%04" PRIx64 "%s ", cc_addr, i, cc_reset); /* Print address. */
        fprintf(f, "%s0b", cc_opcode); /* Print opcode. */
        bitdump(opc & 127, f); /* Print opcode. */
        fprintf(f, "%s ", cc_reset); /* Print opcode. */
        fprintf(f, "%s0x%08" PRIx32 " %s", cc_encoding, self->p[i], cc_reset); /* Print encoding. */
        bci_dump_instr(self->p[i], f, colored);
        if (opc == OPC_LDC) {
            record_t value;
            rtag_t tag;
            if (metaspace_get(&self->pool, bci_mod1unpack_umm24(self->p[i]), &value, &tag)) {
                fprintf(f, "%s ; ", cc_comment);
                switch (tag) {
                    case RT_INT: fprintf(f, value.as_uint > 0xffff ? ("int 0x%" PRIx64) : ("int %" PRIi64), value.as_int); break;
                    case RT_FLOAT: fprintf(f, "float %f", value.as_float); break;
                    case RT_CHAR: fprintf(f, "char %c", value.as_char); break;
                    case RT_BOOL: fprintf(f, "bool %s", value.as_bool ? "true" : "false"); break;
                    case RT_REF: fprintf(f, "ref %p", value.as_ref); break;
                    default: neo_panic("Invalid record tag: %" PRIu8, tag);
                }
                fprintf(f, "%s", cc_reset);
            }
        } else if (opc == OPC_SYSCALL) {
            umm24_t syscall_idx = bci_mod1unpack_umm24(self->p[i]);
            neo_assert(syscall_idx < SYSCALL__LEN);
            fprintf(f, "%s ; %s", cc_comment, syscall_mnemonic[syscall_idx]);
            fprintf(f, "%s", cc_reset);
        }
        fputc('\n', f);
    }
    for (int i = 0; i < 64; ++i) { fputc('-', f); }
    fputc('\n', f);
}

bool bc_validate(const bytecode_t *self, const vm_isolate_t *isolate) {
    neo_assert(isolate && self);
    const bci_instr_t *code = self->p;
    size_t len = self->len;
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
        opcode_t opc = bci_unpackopc(code[i]);
        switch (opc) { /* Specific instruction validation. */
            case OPC_SYSCALL: {
                umm24_t syscall_idx = bci_mod1unpack_umm24(code[i]);
                if (neo_unlikely(syscall_idx >= SYSCALL__LEN)) { /* Check if constant pool index is valid. */
                    neo_error("invalid syscall index: %" PRIi32, syscall_idx);
                    return false;
                }
            } break;
            case OPC_LDC: {
                umm24_t slot_idx = bci_mod1unpack_umm24(code[i]);
                if (neo_unlikely(slot_idx >= self->pool.len)) { /* Check if constant pool index is valid. */
                    neo_error("invalid constant pool slot index: %" PRIi32, slot_idx);
                    return false;
                }
            } break;
            default: ;
        }
    }
    return true;
}

void bc_emit_ipush(bytecode_t *self, neo_int_t x) {
    neo_dassert(self);
    switch (x) { /* Try to use optimized instructions for constant K. */
        case 0: bc_emit(self, OPC_IPUSH0); return;
        case 1: bc_emit(self, OPC_IPUSH1); return;
        case 2: bc_emit(self, OPC_IPUSH2); return;
        case -1: bc_emit(self, OPC_IPUSHM1); return;
        default:
            if (bci_fits_i24(x)) { /* If x fits into 24 bits, use IPUSH. */
                bc_emit(self, bci_comp_mod1_imm24(OPC_IPUSH, (imm24_t)x));
                return;
            } else { /* Otherwise, use LDC to load x from metaspace. */
                cpkey_t key = metaspace_insert_kv(&self->pool, RT_INT, (record_t) {.as_int = x});
                bc_emit(self, bci_comp_mod1_umm24(OPC_LDC, key));
            }
    }
}

void bc_emit_fpush(bytecode_t *self, neo_float_t x) {
    neo_dassert(self);
    if (x == 0.0) { bc_emit(self, OPC_FPUSH0); return; }  /* Try to use optimized instructions for constant K. */
    else if (x == 1.0) { bc_emit(self, OPC_FPUSH1); return; } /* TODO: Maybe used ULP comparison? */
    else if (x == 2.0) { bc_emit(self, OPC_FPUSH2); return; }
    else if (x == 0.5) { bc_emit(self, OPC_FPUSH05); return; }
    else if (x == -1.0) { bc_emit(self, OPC_FPUSHM1); return; }
    else {
        cpkey_t key = metaspace_insert_kv(&self->pool, RT_FLOAT, (record_t) {.as_float = x});
        bc_emit(self, bci_comp_mod1_umm24(OPC_LDC, key));
    }
}
