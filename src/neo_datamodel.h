/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_DATAMODEL_H
#define NEO_DATAMODEL_H

#include "neo_core.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtag_t { /* Record type tag. */
    RT_INT = 0,
    RT_FLOAT,
    RT_CHAR,
    RT_BOOL,
    RT_REF,
    RT__LEN
} rtag_t;
neo_static_assert(RT__LEN <= 255);

typedef union NEO_ALIGN(8) record_t { /* Represents an undiscriminated value in the VM/CP. Either a value type of reference to a class. */
    neo_int_t as_int;
    neo_uint_t as_uint;
    neo_float_t as_float;
    neo_char_t as_char;
    neo_bool_t as_bool;
    void *as_ref; /* Reference type. */
    uint8_t raw[sizeof(neo_int_t)];
} record_t;
neo_static_assert(sizeof(record_t) == 8);

extern NEO_EXPORT bool record_eq(record_t a, record_t b, rtag_t tag);

typedef uint32_t cpkey_t; /* Constant pool index key. */
#define CONSTPOOL_MAX BCI_MOD1UMM24MAX /* Maximum constant pool index, because the ldc immediate is an 24-bit unsigned integer. */
typedef struct constpool_t {
    record_t *p;
    uint8_t /*rtag_t*/ *tags;
    uint32_t len;
    uint32_t cap;
} constpool_t;

extern NEO_EXPORT void constpool_init(constpool_t *self, uint32_t cap /* capacity or 0 */);
extern NEO_EXPORT cpkey_t constpool_put(constpool_t *self, rtag_t tag, record_t value);
extern NEO_EXPORT bool constpool_has(const constpool_t *self, cpkey_t idx);
extern NEO_EXPORT bool constpool_get(constpool_t *self, cpkey_t idx, record_t *value, rtag_t *tag);
extern NEO_EXPORT void constpool_free(constpool_t *self);

#ifdef __cplusplus
}
#endif

#endif
