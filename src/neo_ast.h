/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AST (Abstract Syntax Tree) definition and implementation */

#ifndef NEO_AST_H
#define NEO_AST_H

#include "neo_core.h"
#include "neo_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** ~~~~~ Some delightful words about the AST memory representation ~~~~~
** The 'astnode_t' struct represents a single AST node.
** It contains a tag or discriminator 'type' which is used to determine the type of the node.
** The data of the individual node types is stored in the 'dat' union.
** Because allocating all nodes on the heap would be slow and encourage heap fragmentation,
** we use a memory pool 'astpool_t' to allocate them.
** IMPORTANT:
** 1. The pool might reallocate, so holding a pointer to a node or any data inside the node is not safe.
**    This is the same as in C++, where you cannot store iterators to vector elements, because they might reallocate.
**    -> So instead of storing a pointer, we store an index into the pool 'astref_t',
**       which can be resolved into the actual node pointer using 'astref_resolve()',
**       but this pointer should not be kept alive for long.
**       The astref_t can also be ASTREF_NULL to indicate a null pointer. This is useful for optional nodes.
** 2. The pool is thread local and all nodes are deallocated when the pool is freed.
** 3. For blocks a list of astref_t is used to store the child nodes. This list is also allocated on the pool.
**    -> To allocate and resolved lists of astref_t we use the listref_t type and the 'astref_resolve_list()' function.
 *       This works exactly the same way as astref_t and 'astref_resolve()'.
 *       Remember that is also not safe to store pointers to the list elements neither the list pointer itself.
*/

/* Reference to AST-node or AST-node-list as index (1-based) into the AST memory pool. */
typedef uint32_t astref_t, listref_t;
#define astref_decl(type) astref_t /* req = required, opt = optional. */
#define ASTREF_NULL (0u) /* Null reference yields NULL pointer when resolved. */
#define astref_isnull(ref) ((ref)==ASTREF_NULL) /* Check if reference is null. */

typedef struct astnode_t astnode_t; /* Node. */
typedef struct astpool_t astpool_t; /* AST memory pool. */
struct error_vector_t;

typedef struct node_error_t {
    const char *message;
    token_t token;
} node_error_t;

typedef struct node_group_t {
    astref_decl(req) child_expr;
} node_group_t;

typedef struct node_int_literal_t {
    neo_int_t value;
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_int_literal_t;

typedef struct node_float_literal_t {
    neo_float_t value;
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_float_literal_t;

typedef struct node_char_literal_t {
    neo_char_t value;
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_char_literal_t;

typedef struct node_bool_literal_t {
    neo_bool_t value;
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_bool_literal_t;

typedef struct node_string_literal_t {
    uint8_t *str; /* String literals own the memory, because the literal must be escaped :) TODO: add string pool */
    uint32_t hash;
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_string_literal_t;

typedef struct node_ident_literal_t {
    srcspan_t span;
    uint32_t hash; /* Auto-generated hash from span. */
    token_t tok; /* Token for better error handling in the semantic analysis stage. */
} node_ident_literal_t;

/* Symbol table entry data structure. */
typedef struct symrecord_t {
    token_t tok;
    astref_t node;
} symrecord_t;

/* Symbol table bucket data structure. */
typedef struct symbuck_t symbuck_t;
struct symbuck_t {
    node_ident_literal_t key;
    symrecord_t val;
};

/* Symbol table hashmap data structure which associates identifier spans as keys with symbols as values in constant time. */
typedef struct symtab_t {
    symbuck_t *buckets;
    uint32_t len;
    uint32_t cap;
} symtab_t;

#define symtab_is_init(self) ((self)->buckets != NULL)
extern NEO_EXPORT void symtab_init(symtab_t *self, uint32_t cap);
extern NEO_EXPORT bool symtab_put(symtab_t *self, const node_ident_literal_t *key, const symrecord_t *val);
extern NEO_EXPORT bool symtab_get(symtab_t *self, const node_ident_literal_t *key, const symrecord_t **val);
extern NEO_EXPORT uint32_t symtab_len(const symtab_t *self);
extern NEO_EXPORT void symtab_iter(const symtab_t *self, void (*callback)(const node_ident_literal_t *key, const symrecord_t *val, void *usr), void *usr);
extern NEO_EXPORT void symtab_free(symtab_t *self);
extern NEO_EXPORT NEO_COLDPROC void symtab_print(const symtab_t *self, FILE *f, const char *name);

typedef enum unary_op_type_t {
    UNOP_PLUS, /* +1. */
    UNOP_MINUS, /* -1. */
    UNOP_LOG_NOT, /* not true. */
    UNOP_BIT_COMPL, /* ~1. */
    UNOP_INC, /* ++x. */
    UNOP_DEC, /* --x. */
    UNOP__COUNT
} unary_op_type_t;
neo_static_assert(UNOP__COUNT < 64);
#define UNOP_ASSIGN_MASK ((1ull<<(UNOP_INC&63))|(1ull<<(UNOP_DEC&63))) /* Mask of all assigning operators. */
neo_static_assert(UNOP_ASSIGN_MASK <= UINT64_MAX);
extern NEO_EXPORT const char *unary_op_lexeme(unary_op_type_t op);

typedef struct node_unary_op_t {
    unary_op_type_t opcode : 8;
    astref_decl(req) child_expr;
} node_unary_op_t;

typedef enum binary_op_type_t {
    BINOP_DOT,
    BINOP_ASSIGN,
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_POW,
    BINOP_ADD_NO_OV,
    BINOP_SUB_NO_OV,
    BINOP_MUL_NO_OV,
    BINOP_POW_NO_OV,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_ADD_ASSIGN,
    BINOP_SUB_ASSIGN,
    BINOP_MUL_ASSIGN,
    BINOP_POW_ASSIGN,
    BINOP_ADD_ASSIGN_NO_OV,
    BINOP_SUB_ASSIGN_NO_OV,
    BINOP_MUL_ASSIGN_NO_OV,
    BINOP_POW_ASSIGN_NO_OV,
    BINOP_DIV_ASSIGN,
    BINOP_MOD_ASSIGN,
    BINOP_EQUAL,
    BINOP_NOT_EQUAL,
    BINOP_LESS,
    BINOP_LESS_EQUAL,
    BINOP_GREATER,
    BINOP_GREATER_EQUAL,
    BINOP_BIT_AND,
    BINOP_BIT_OR,
    BINOP_BIT_XOR,
    BINOP_BIT_AND_ASSIGN,
    BINOP_BIT_OR_ASSIGN,
    BINOP_BIT_XOR_ASSIGN,
    BINOP_BIT_ASHL,
    BINOP_BIT_ASHR,
    BINOP_BIT_ROL,
    BINOP_BIT_ROR,
    BINOP_BIT_LSHR,
    BINOP_BIT_ASHL_ASSIGN,
    BINOP_BIT_ASHR_ASSIGN,
    BINOP_BIT_ROL_ASSIGN,
    BINOP_BIT_ROR_ASSIGN,
    BINOP_BIT_LSHR_ASSIGN,
    BINOP_LOG_AND,
    BINOP_LOG_OR,
    BINOP_CALL,
    BINOP__COUNT
} binary_op_type_t;
neo_static_assert(BINOP__COUNT < 64);
#define BINOP_ASSIGN_MASK\
        ((1ull<<(BINOP_ASSIGN&63))|\
        (1ull<<(BINOP_ADD_ASSIGN)&63)|\
        (1ull<<(BINOP_SUB_ASSIGN)&63)|\
        (1ull<<(BINOP_MUL_ASSIGN)&63)|\
        (1ull<<(BINOP_POW_ASSIGN)&63)|\
        (1ull<<(BINOP_ADD_ASSIGN_NO_OV)&63)|\
        (1ull<<(BINOP_SUB_ASSIGN_NO_OV)&63)|\
        (1ull<<(BINOP_MUL_ASSIGN_NO_OV)&63)|\
        (1ull<<(BINOP_POW_ASSIGN_NO_OV)&63)|\
        (1ull<<(BINOP_DIV_ASSIGN)&63)|\
        (1ull<<(BINOP_MOD_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_AND_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_OR_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_XOR_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_ASHL_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_ASHR_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_ROL_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_ROR_ASSIGN)&63)|\
        (1ull<<(BINOP_BIT_LSHR_ASSIGN)&63)) /* Mask of all assigning operators. */
neo_static_assert(BINOP_ASSIGN_MASK <= UINT64_MAX);
extern NEO_EXPORT const char *binary_op_lexeme(binary_op_type_t op);

typedef struct node_binary_op_t {
    binary_op_type_t opcode : 8;
    astref_decl(req) left_expr;
    astref_decl(req) right_expr;
} node_binary_op_t;

typedef struct node_method_t {
    astref_decl(req) ident;
    astref_decl(opt) params;
    astref_decl(opt) ret_type;
    astref_decl(opt) body;
} node_method_t;

typedef enum block_scope_t {
    BLOCKSCOPE_MODULE,          /* Module (per-file). */
    BLOCKSCOPE_CLASS,           /* Class body. */
    BLOCKSCOPE_LOCAL,           /* Method or statement body. */
    BLOCKSCOPE_PARAMLIST,       /* Method parameter list. */
    BLOCKSCOPE_ARGLIST,         /* Method argument expr list. */
    BLOCKSCOPE__COUNT
} block_scope_t;

typedef struct block_symbol_t {
    node_ident_literal_t ident;
    astref_t node;
    token_t token;
} block_symbol_t;

/*
** Represents a module/class/local/param scope which includes
** 1. The child statements
** 2. A symbol table for the identifiers, generated in the semantic analysis stage.
** ! When adding/removing symbol tables or blockscopes, remember to free them correctly in the block functions.
*/
typedef struct node_block_t {
    block_scope_t scope : 8; /* Discriminator. */
    union {
        struct {
            symtab_t class_table; /* Global symbol table. */
            symtab_t variable_table; /* Class variables (static and local). */
            symtab_t method_table; /* Class methods (static and local). */
        } sc_module; /* Scope of: BLOCKSCOPE_MODULE */
        struct {
            symtab_t variable_table; /* Class variables (static and local). */
            symtab_t method_table; /* Class methods (static and local). */
        } sc_class; /* Scope of: BLOCKSCOPE_CLASS */
        struct {
            symtab_t variable_table; /* Class variables (static and local). */
        } sc_local; /* Scope of: BLOCKSCOPE_LOCAL */
        struct {
            symtab_t variable_table; /* Local parameter variables. */
        } sc_params; /* Scope of: BLOCKSCOPE_PARAMLIST */
    } symtabs; /* The symbol tables are initialized and populated in the semantic analysis stage, we only reserve storage for them. */
    listref_t nodes; /* Child nodes. */
    uint32_t len;
    uint32_t cap;
    uint32_t scope_depth;
} node_block_t;
#define node_block_can_have_symtabs(self) ((self)->scope != BLOCKSCOPE_ARGLIST) /* All scopes have symtabs, except BLOCKSCOPE_ARGLIST. */
extern NEO_EXPORT void node_block_push_child(astpool_t *pool, node_block_t *self, astref_decl(req) node);

/* Variable type */
typedef enum variable_scope_t {
    VARSCOPE_LOCAL,          /* Local variable. */
    VARSCOPE_PARAM,          /* Local function parameter. */
    VARSCOPE_STATIC_FIELD,   /* Static class field. */
    VARSCOPE_FIELD,          /* Class field. */
    VARSCOPE__COUNT
} variable_scope_t;

typedef struct node_variable_t {
    variable_scope_t var_scope : 8; /* Variable type. */
    astref_decl(req) ident;      /* Required variable callee_ident. */
    astref_decl(req) type;       /* Required variable type. */
    astref_decl(req) init_expr;  /* Required variable initializer. */
} node_variable_t;

typedef struct node_return_t {
    astref_decl(opt) child_expr;
} node_return_t;

typedef struct node_branch_t {
    astref_decl(req) cond_expr;
    astref_decl(req) true_block;
    astref_decl(opt) false_block;
} node_branch_t;

typedef struct node_loop_t {
    astref_decl(req) cond_expr;
    astref_decl(req) true_block;
} node_loop_t;

typedef struct node_class_t {
    astref_decl(req) ident;
    astref_decl(opt) body;
} node_class_t;

typedef struct node_module_t {
    astref_decl(opt) ident;
    astref_decl(opt) body;
} node_module_t;

#define nodedef(_, __)/* Leaf-nodes first. */\
    _(ASTNODE_ERROR, "ERROR")__\
    _(ASTNODE_BREAK, "BREAK")__\
    _(ASTNODE_CONTINUE, "CONTINUE")__\
    _(ASTNODE_INT_LIT, "INT")__\
    _(ASTNODE_FLOAT_LIT, "FLOAT")__\
    _(ASTNODE_CHAR_LIT, "CHAR")__\
    _(ASTNODE_BOOL_LIT, "BOOL")__\
    _(ASTNODE_STRING_LIT, "STRING")__ \
    _(ASTNODE_IDENT_LIT, "IDENT")__\
    _(ASTNODE_SELF_LIT, "SELF")__\
    _(ASTNODE_GROUP, "GROUP")__\
    _(ASTNODE_UNARY_OP, "UNARY OP")__\
    _(ASTNODE_BINARY_OP, "BINARY OP")__\
    _(ASTNODE_FUNCTION, "FUNCTION")__\
    _(ASTNODE_BLOCK, "BLOCK")__\
    _(ASTNODE_VARIABLE, "VARIABLE")__\
    _(ASTNODE_RETURN, "RETURN")__\
    _(ASTNODE_BRANCH, "BRANCH")__\
    _(ASTNODE_LOOP, "LOOP")__\
    _(ASTNODE_CLASS, "CLASS")__\
    _(ASTNODE_MODULE, "MODULE")

#define _(_1, _2) _1
typedef enum astnode_type_t {
    nodedef(_, NEO_SEP),
    ASTNODE__COUNT
} astnode_type_t;
#undef _
neo_static_assert(ASTNODE__COUNT <= 63);
#define astmask(t) (1ull<<((t)&63))
#define ASTNODE_HULL_MASK (astmask(ASTNODE_BREAK)|astmask(ASTNODE_CONTINUE)|astmask(ASTNODE_SELF_LIT))  /* Hulls are nodes which have no data associated with them. E.g. break has no children or data. */
#define ASTNODE_LITERAL_MASK (astmask(ASTNODE_INT_LIT)|astmask(ASTNODE_FLOAT_LIT)|astmask(ASTNODE_CHAR_LIT)|astmask(ASTNODE_BOOL_LIT)|astmask(ASTNODE_STRING_LIT)|astmask(ASTNODE_IDENT_LIT)|astmask(ASTNODE_SELF_LIT)) /* All literal types. */
#define ASTNODE_EXPR_MASK (ASTNODE_LITERAL_MASK|astmask(ASTNODE_UNARY_OP)|astmask(ASTNODE_BINARY_OP)|astmask(ASTNODE_GROUP)) /* All expression types. */
#define ASTNODE_LEAF_MASK (ASTNODE_HULL_MASK|ASTNODE_LITERAL_MASK|astmask(ASTNODE_ERROR)) /* Leafs are nodes which have no further children. Logically, all hulls are also leafs. */
#define ASTNODE_CONTROL_FLOW (astmask(ASTNODE_BRANCH)|astmask(ASTNODE_RETURN)|astmask(ASTNODE_LOOP)|astmask(ASTNODE_BREAK)|astmask(ASTNODE_CONTINUE)) /* And also the CALL binary expression. */
neo_static_assert(ASTNODE_HULL_MASK <= UINT64_MAX);
neo_static_assert(ASTNODE_LITERAL_MASK <= UINT64_MAX);
neo_static_assert(ASTNODE_EXPR_MASK <= UINT64_MAX);
neo_static_assert(ASTNODE_LEAF_MASK <= UINT64_MAX);
neo_static_assert(ASTNODE_CONTROL_FLOW <= UINT64_MAX);
extern const char *const astnode_names[ASTNODE__COUNT];

struct astnode_t {
    astnode_type_t type : 8;
    union { /* Leaf-nodes first, order as in ASTNODE_*. */
        node_error_t n_error; /* Leaf-node. */
        node_int_literal_t n_int_lit; /* Leaf-node. */
        node_float_literal_t n_float_lit; /* Leaf-node. */
        node_char_literal_t n_char_lit; /* Leaf-node. */
        node_bool_literal_t n_bool_lit; /* Leaf-node. */
        node_string_literal_t n_string_lit; /* Leaf-node. */
        node_ident_literal_t n_ident_lit; /* Leaf-node. */
        node_group_t n_group;
        node_unary_op_t n_unary_op;
        node_binary_op_t n_binary_op;
        node_method_t n_method;
        node_block_t n_block;
        node_variable_t n_variable;
        node_return_t n_return;
        node_branch_t n_branch;
        node_loop_t n_loop;
        node_class_t n_class;
        node_module_t n_module;
        /* ASTNODE_SELF_LIT, ASTNODE_BREAK etc.. have no data associated with them and therefore no custom node. */
    } dat;
};

/* AST allocation routines. */
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_error(astpool_t *pool, const node_error_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_group(astpool_t *pool, const node_group_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_int(astpool_t *pool, neo_int_t value, const token_t *tok);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_float(astpool_t *pool, neo_float_t value, const token_t *tok);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_char(astpool_t *pool, neo_char_t value, const token_t *tok);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_bool(astpool_t *pool, neo_bool_t value, const token_t *tok);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_string(astpool_t *pool, const uint8_t *str, const token_t *tok);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_ident(astpool_t *pool, srcspan_t value, const token_t *tok); /* Contains the token for better error handling in the semantic analysis stage. */
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_unary_op(astpool_t *pool, const struct node_unary_op_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_binary_op(astpool_t *pool, const node_binary_op_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_method(astpool_t *pool, const node_method_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_block(astpool_t *pool, const node_block_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_block_with_nodes(astpool_t *pool, block_scope_t type, astref_t *nodes); /* Note: Assign ASTREF_NULL as last element (terminator) in the nodes array! */
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_variable(astpool_t *pool, const node_variable_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_return(astpool_t *pool, const node_return_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_break(astpool_t *pool);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_continue(astpool_t *pool);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_branch(astpool_t *pool, const node_branch_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_loop(astpool_t *pool, const node_loop_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_class(astpool_t *pool, const node_class_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_module(astpool_t *pool, const node_module_t *node);
extern NEO_EXPORT NEO_NODISCARD astref_t astnode_new_self(astpool_t *pool);

extern NEO_EXPORT size_t astnode_visit(astpool_t *pool, astref_t root, void (*visitor)(astpool_t *pool, astref_t node, void *user), void *user); /* Visits AST tree in depth-first order. Returns the amount of nodes visited. */

struct astpool_t {
    neo_mempool_t node_pool; /* Stores the astnode_t objects. */
    neo_mempool_t list_pool; /* Stores lists of astref_t objects. */
};
extern NEO_EXPORT void astpool_init(astpool_t *self);
extern NEO_EXPORT void astpool_free(astpool_t *self);
extern NEO_EXPORT void astpool_reset(astpool_t *self); /* Resets the astpool_t object to allow reusing the capacity. */
extern NEO_EXPORT NEO_NODISCARD astref_t astpool_alloc(astpool_t *self, astnode_t **o, astnode_type_t type);
extern NEO_EXPORT NEO_NODISCARD listref_t astpool_alloclist(astpool_t *self, astref_t **o, uint32_t len);
static NEO_AINLINE NEO_NODISCARD bool astpool_isvalidref(const astpool_t *self, astref_t ref) {
    neo_dassert(self != NULL, "self is NULL");
    return neo_likely(!astref_isnull(ref) && ((size_t)ref*sizeof(astnode_t))-sizeof(astnode_t) < self->node_pool.len);
}
static NEO_AINLINE NEO_NODISCARD bool astpool_isvalidlistref(const astpool_t *self, listref_t ref) {
    neo_dassert(self != NULL, "self is NULL");
    return neo_likely((size_t)ref*sizeof(astref_t) < self->list_pool.len); /* Remember: list refs cannot be ASTREF_NULL, as they are not required to be nullable. */
}
/* Resolves AST reference to node pointer! Do NOT keep the node pointer alive, it might be invalidated on reallocation.
** (E.g. Same rule applies to std::vector in C++ when storing iterators and then pushing elements.)
*/
static NEO_AINLINE NEO_NODISCARD astnode_t *astpool_resolve(const astpool_t *self, astref_t ref) {
    neo_dassert(self != NULL, "self is NULL");
    neo_dassert(self->node_pool.cap > self->node_pool.len, "Pool is full.");
#if NEO_DBG
    if (!astref_isnull(ref)) {
        neo_dassert(astpool_isvalidref(self, ref), "Invalid AST reference.");
    }
#endif
    return neo_unlikely(astref_isnull(ref)) ? NULL : neo_mempool_getelementptr(self->node_pool, ref-1, astnode_t); /* refs start at 1, 0 is reserved for NULL */
}
static NEO_AINLINE NEO_NODISCARD astref_t *astpool_resolvelist(const astpool_t *self, listref_t ref) {
    neo_dassert(self != NULL, "self is NULL");
    neo_dassert(self->node_pool.cap > self->node_pool.len, "Pool is full.");
    neo_assert(astpool_isvalidlistref(self, ref), "Invalid list reference.");
    return neo_mempool_getelementptr(self->list_pool, ref, astref_t); /* Remember: list refs cannot be ASTREF_NULL, as they are not required to be nullable. */
}

#ifdef NEO_EXTENSION_AST_RENDERING
    extern NEO_EXPORT void ast_node_graphviz_dump(astpool_t *pool, astref_t root, FILE *f); /* Dumps AST tree as Graphviz code which can be then visualized.  */
    extern NEO_EXPORT void ast_node_graphviz_render(astpool_t *pool, astref_t root, const char *filename); /* Renders AST tree as jpg image using Graphviz. */
#endif

#ifdef __cplusplus
}
#endif
#endif
