/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_ast.h"

/* Implement AST node factory methods */

#define impl_ast_node_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool, const node_##name##_t *node) {\
    neo_dassert(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(*node));\
    nn->type = ASTNODE_##ttype;\
    nn->dat.n_##name = *node;\
    return nn;\
  }

#define impl_ast_node_hull_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool) {\
    neo_dassert(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));\
    nn->type = ASTNODE_##ttype;\
    return nn;\
  }

#define impl_ast_node_literal_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool, neo_##name##_t value) {\
    neo_dassert(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));\
    nn->type = ASTNODE_##ttype;\
    nn->dat.n_##name##_lit.value = value;\
    return nn;\
  }

impl_ast_node_factory(error, ERROR)
impl_ast_node_factory(group, GROUP)
impl_ast_node_factory(unary_op, UNARY_OP)
impl_ast_node_factory(binary_op, BINARY_OP)
impl_ast_node_factory(method, METHOD)
impl_ast_node_factory(block, BLOCK)
impl_ast_node_factory(variable, VARIABLE)
impl_ast_node_factory(return, RETURN)
impl_ast_node_hull_factory(break, BREAK)
impl_ast_node_hull_factory(continue, CONTINUE)
impl_ast_node_factory(branch, BRANCH)
impl_ast_node_factory(loop, LOOP)
impl_ast_node_factory(class, CLASS)
impl_ast_node_factory(module, MODULE)

impl_ast_node_literal_factory(int, INT_LIT)
impl_ast_node_literal_factory(float, FLOAT_LIT)
impl_ast_node_literal_factory(char, CHAR_LIT)
impl_ast_node_literal_factory(bool, BOOL_LIT)

astnode_t *astnode_new_string(neo_mempool_t *pool, srcspan_t value) {
    neo_dassert(pool);
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));
    nn->type = ASTNODE_STRING_LIT;
    nn->dat.n_string_lit.span = value;
    nn->dat.n_string_lit.hash = srcspan_hash(value);
    return nn;
}

astnode_t *astnode_new_ident(neo_mempool_t *pool, srcspan_t value) {
    astnode_t *nn = astnode_new_string(pool, value);
    nn->type = ASTNODE_IDENT_LIT;
    return nn;
}

static const uint64_t NEO_UNUSED ast_node_block_masks[BLOCK__COUNT] = { /* This table contains masks of the allowed ASTNODE_* types for each block type inside a node_block_t. */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_CLASS), /* BLOCK_MODULE */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_METHOD)|astmask(ASTNODE_VARIABLE), /* BLOCK_CLASS */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE)|astmask(ASTNODE_BRANCH) /* BLOCK_LOCAL */
    |astmask(ASTNODE_LOOP)|astmask(ASTNODE_UNARY_OP)|astmask(ASTNODE_BINARY_OP)|astmask(ASTNODE_GROUP) /* BLOCK_LOCAL */
    |astmask(ASTNODE_RETURN)|astmask(ASTNODE_BREAK)|astmask(ASTNODE_CONTINUE), /* BLOCK_LOCAL */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE) /* BLOCK_PARAMLIST */
};

void node_block_push_child(neo_mempool_t *pool, node_block_t *block, astnode_t *node) {
    neo_dassert(pool && block);
    if (neo_unlikely(!node)) {
        return;
    } else if (!block->nodes || !block->len) { /* No nodes yet, so allocate. */
        block->len = 1;
        block->nodes = neo_mempool_alloc(pool, (block->cap=1<<8)*sizeof(astnode_t *));
        *block->nodes = node;
    } else if (block->len >= block->cap) { /* Reallocate if necessary. */
        block->nodes = neo_mempool_alloc(pool, (block->cap<<=2)*sizeof(astnode_t *)); /* Wasting a lot of memory here, because we can't individually free the previous block. :/ */
        block->nodes[block->len++] = node;
    } else { /* Otherwise, just push. */
        block->nodes[block->len++] = node;
    }
}

#undef impl_ast_node_literal_factory
#undef impl_ast_node_hull_factory
#undef impl_ast_node_factory

static void astnode_visit_root_impl(astnode_t *root, void (*visitor)(astnode_t *node, void *user), void *user, size_t *c) {
    if (neo_unlikely(!root)) { return; } /* Skip NULL nodes. */
    neo_dassert(visitor && c);
    ++*c; /* Increment counter. */
    switch (root->type) { /* Leafs have no children, so they are skipped. */
        case ASTNODE_ERROR:
        case ASTNODE_BREAK:
        case ASTNODE_CONTINUE:
        case ASTNODE_INT_LIT:
        case ASTNODE_FLOAT_LIT:
        case ASTNODE_CHAR_LIT:
        case ASTNODE_BOOL_LIT:
        case ASTNODE_STRING_LIT:
        case ASTNODE_IDENT_LIT: {
            neo_dassert(!!(ASTNODE_LEAF_MASK&astmask(root->type)));
        } return; /* Visitor invocation is redundant. */
        case ASTNODE_GROUP: {
            const node_group_t *data = &root->dat.n_group;
            astnode_visit_root_impl(data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &root->dat.n_unary_op;
            astnode_visit_root_impl(data->expr, visitor, user, c);
        } break;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &root->dat.n_binary_op;
            astnode_visit_root_impl(data->left_expr, visitor, user, c);
            astnode_visit_root_impl(data->right_expr, visitor, user, c);
        } break;
        case ASTNODE_METHOD: {
            const node_method_t *data = &root->dat.n_method;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->params, visitor, user, c);
            astnode_visit_root_impl(data->ret_type, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &root->dat.n_block;
            for (uint32_t i = 0; i < data->len; ++i) {
                astnode_visit_root_impl(data->nodes[i], visitor, user, c);
            }
        } break;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &root->dat.n_variable;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->type, visitor, user, c);
            astnode_visit_root_impl(data->init_expr, visitor, user, c);
        } break;
        case ASTNODE_RETURN: {
            const node_return_t *data = &root->dat.n_return;
            astnode_visit_root_impl(data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &root->dat.n_branch;
            astnode_visit_root_impl(data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(data->true_block, visitor, user, c);
            astnode_visit_root_impl(data->false_block, visitor, user, c);
        } break;
        case ASTNODE_LOOP: {

            const node_loop_t *data = &root->dat.n_loop;
            astnode_visit_root_impl(data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(data->true_block, visitor, user, c);
        } break;
        case ASTNODE_CLASS: {
            const node_class_t *data = &root->dat.n_class;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;
        case ASTNODE_MODULE: {
            const node_module_t *data = &root->dat.n_module;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;
        default: {
            neo_panic("invalid node type: %d", root->type);
        }
    }
    (*visitor)(root, user);
}

size_t astnode_visit(astnode_t *root, void (*visitor)(astnode_t *node, void *user), void *user) {
    size_t c = 0;
    astnode_visit_root_impl(root, visitor, user, &c);
    return c;
}

#define astverify(expr, msg) neo_assert((expr)&&"AST verification failed: " msg)
#define isexpr(node) (ASTNODE_EXPR_MASK&(astmask((node)->type)))

static void ast_validator(astnode_t *node, void *user) {
    (void)user;
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
            astverify(data->message, "Error message is NULL");
            astverify(data->token.lexeme.p && data->token.lexeme.len, "Error token lexeme is NULL");
        } return;
        case ASTNODE_BREAK: /* No assoc data. */
        case ASTNODE_CONTINUE: /* No assoc data. */
        case ASTNODE_INT_LIT: /* No validation needed. */
        case ASTNODE_FLOAT_LIT: /* No validation needed. */
        case ASTNODE_CHAR_LIT: /* No validation needed. */
        case ASTNODE_BOOL_LIT: /* No validation needed. */
        case ASTNODE_STRING_LIT: /* No validation needed. */
        case ASTNODE_IDENT_LIT: /* No validation needed. */
            return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
            if (data->child_expr != NULL) {
                astverify(isexpr(data->child_expr), "Group child is not an expression");
            }
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
            astverify(data->expr != NULL, "Unary op child is NULL");
            astverify(isexpr(data->expr), "Unary op child is not an expression");
            astverify(data->type <= UNOP__COUNT, "Unary op operator is invalid");
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
            astverify(data->left_expr != NULL, "Binary op left is NULL");
            astverify(isexpr(data->left_expr), "Binary op left is not an expression");
            astverify(data->right_expr != NULL, "Binary op right is NULL");
            astverify(isexpr(data->right_expr), "Binary op right is not an expression");
            astverify(data->type <= BINOP__COUNT, "Binary op operator is invalid");
        } return;
        case ASTNODE_METHOD: {
            const node_method_t *data = &node->dat.n_method;
            astverify(data->ident != NULL, "Method ident is NULL");
            astverify(data->ident->type == ASTNODE_IDENT_LIT, "Method ident is not an identifier");
            if (data->params) { /* Optional. */
                astverify(data->params->type == ASTNODE_BLOCK, "Method params is not a block");
                astverify(data->body->dat.n_block.type == BLOCK_PARAMLIST, "Method params is not a param-list block");
            }
            if (data->ret_type) { /* Optional. */
                astverify(data->ret_type->type == ASTNODE_IDENT_LIT, "Method return type is not an identifier");
            }
            if (data->body) { /* Optional. */
                astverify(data->body->type == ASTNODE_BLOCK, "Method body is not a block");
                astverify(data->body->dat.n_block.type == BLOCK_LOCAL, "Method body is not a local block");
            }
        } return;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &node->dat.n_block;
            astverify(data->nodes != NULL, "Block nodes array is NULL");
            astverify(data->len > 0, "Block nodes array is empty");
            for (uint32_t i = 0; i < data->len; ++i) {
                const astnode_t *child = data->nodes[i];
                astverify(child != NULL, "Block node is NULL");
                uint64_t mask = ast_node_block_masks[data->type];
                astverify(mask&astmask(node->type), "Block node type is not allowed in this block kind"); /* Check that the node type is allowed in this block type. For example, method declarations are not allowed in parameter list blocks.  */
            }
            switch (data->type) {
                case BLOCK_MODULE: {
                    const symtab_t *class_table = data->symtabs.sc_module.class_table;
                    astverify(class_table != NULL, "Module class table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_CLASS: {
                    const symtab_t *var_table = data->symtabs.sc_class.var_table;
                    astverify(var_table != NULL, "Class variable table is NULL");
                    const symtab_t *method_table = data->symtabs.sc_class.method_table;
                    astverify(method_table != NULL, "Class method table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_LOCAL: {
                    const symtab_t *var_table = data->symtabs.sc_local.var_table;
                    astverify(var_table != NULL, "Local variable table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_PARAMLIST: {
                    const symtab_t *var_table = data->symtabs.sc_params.var_table;
                    astverify(var_table != NULL, "Parameter list variable table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                default: neo_panic("Invalid block type: %d", data->type);
            }
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
            astverify(data->ident != NULL, "Variable ident is NULL");
            astverify(data->ident->type == ASTNODE_IDENT_LIT, "Variable ident is not an identifier");
            astverify(data->type != NULL, "Variable type is NULL");
            astverify(data->type->type == ASTNODE_IDENT_LIT, "Variable type is not an identifier");
            astverify(data->init_expr != NULL, "Variable init expr is NULL");
            astverify(isexpr(data->init_expr), "Variable init expr is not an expression");
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
            if (data->child_expr) {
                astverify(isexpr(data->child_expr), "Return child expr is not an expression");
            }
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
            astverify(data->cond_expr != NULL, "Branch condition expr is NULL");
            astverify(isexpr(data->cond_expr), "Branch condition expr is not an expression");
            astverify(data->true_block != NULL, "Branch true block is NULL");
            astverify(data->true_block->type == ASTNODE_BLOCK, "Branch true block is not a block");
            astverify(data->true_block->dat.n_block.type == BLOCK_LOCAL, "Branch true block is not a local block");
            if (data->false_block) { /* Else-block is optional. */
                astverify(data->false_block->type == ASTNODE_BLOCK, "Branch false block is not a block");
                astverify(data->false_block->dat.n_block.type == BLOCK_LOCAL, "Branch false block is not a local block");
            }
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
            astverify(data->cond_expr != NULL, "Loop condition expr is NULL");
            astverify(isexpr(data->cond_expr), "Loop condition expr is not an expression");
            astverify(data->true_block != NULL, "Loop true block is NULL");
            astverify(data->true_block->type == ASTNODE_BLOCK, "Loop true block is not a block");
            astverify(data->true_block->dat.n_block.type == BLOCK_LOCAL, "Loop true block is not a local block");
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
            astverify(data->ident != NULL, "Class ident is NULL");
            astverify(data->ident->type == ASTNODE_IDENT_LIT, "Class ident is not an identifier");
            if (data->body) {
                astverify(data->body->type == ASTNODE_BLOCK, "Class body is not a block");
                astverify(data->body->dat.n_block.type == BLOCK_CLASS, "Class body is not a class block");
            }
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
            astverify(data->ident != NULL, "Module ident is NULL");
            astverify(data->ident->type == ASTNODE_IDENT_LIT, "Module ident is not an identifier");
            if (data->body) {
                astverify(data->body->type == ASTNODE_BLOCK, "Module body is not a block");
                astverify(data->body->dat.n_block.type == BLOCK_MODULE, "Module body is not a module block");
            }
        } return;
        default: {
            neo_panic("invalid node type: %d", node->type);
        }
    }
}

void astnode_validate(astnode_t *root) {
    astnode_visit(root, &ast_validator, NULL);
}

#if 0 /* Copy and paste this skeleton to quickly create a new AST visitor. */
static void my_ast_validator(astnode_t *node, void *user) {
    (void)user;
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
        } return;
        case ASTNODE_BREAK: {

        } return;
        case ASTNODE_CONTINUE: {

        } return;
        case ASTNODE_INT_LIT: {
            const node_int_literal_t *data = &node->dat.n_int_lit;
        } return;
        case ASTNODE_FLOAT_LIT: {
            const node_float_literal_t *data = &node->dat.n_float_lit;
        } return;
        case ASTNODE_CHAR_LIT: {
            const node_char_literal_t *data = &node->dat.n_char_lit;
        } return;
        case ASTNODE_BOOL_LIT: {
            const node_bool_literal_t *data = &node->dat.n_bool_lit;
        } return;
        case ASTNODE_STRING_LIT: {
            const node_string_literal_t *data = &node->dat.n_string_lit;
        } return;
        case ASTNODE_IDENT_LIT: {
            const node_ident_literal_t *data = &node->dat.n_ident_lit;
        } return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
        } return;
        case ASTNODE_METHOD: {
            const node_method_t *data = &node->dat.n_method;
        } return;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &node->dat.n_block;
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
        } return;
        default: {
            neo_panic("invalid node type: %d", node->type);
        }
    }
}
#endif
