/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <neo_ast.h>

/* These are specified in C to use pointer to compound-literals, which is really nice... */

astnode_t *get_mock_var(neo_mempool_t *pool) {
    return astnode_new_variable(pool, &(node_variable_t) {
        .ident = astnode_new_ident(pool, srcspan_from("test")),
        .type = astnode_new_ident(pool, srcspan_from("int")),
        .init_expr = astnode_new_int(pool, -22),
        .vartype = VARTYPE_LOCAL
    });
}

astnode_t *get_mock_class(neo_mempool_t *pool) {
    return astnode_new_class(pool, &(node_class_t) {
        .ident = astnode_new_ident(pool, srcspan_from("test")),
        .body = astnode_new_block_with_nodes(pool, BLOCK_CLASS, (astnode_t*[]){
            get_mock_var(pool),
            astnode_new_method(pool, &(node_method_t) {
                .ident = astnode_new_ident(pool, srcspan_from("f")),
                .params = NULL,
                .ret_type = astnode_new_ident(pool, srcspan_from("int")),
                .body = astnode_new_block_with_nodes(pool, BLOCK_LOCAL, (astnode_t*[]){
                    get_mock_var(pool),
                    astnode_new_return(pool, &(node_return_t) {
                        .child_expr = astnode_new_binary_op(pool, &(node_binary_op_t) {
                            .type = BINOP_ADD,
                            .left_expr = astnode_new_int(pool, 2),
                            .right_expr = astnode_new_ident(pool, srcspan_from("test"))
                        })
                    }),
                    NULL
                })
            }),
            NULL
        })
    });
}