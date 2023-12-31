// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_ast.h>

TEST(ast, symtab) {
    symtab_t symtab {};
    symtab_init(&symtab, 4);

    for (int i = 0; i < 0xfff; ++i) {
        ASSERT_EQ(symtab.len, i);

        std::string val{std::to_string(i)};
        srcspan_t span {
            .p = reinterpret_cast<const std::uint8_t *>(val.c_str()),
            .len = static_cast<std::uint32_t>(val.size())
        };

        node_ident_literal_t key {
            .span = span,
            .hash = srcspan_hash(span)
        };

        symrecord_t value {
            .tok = {},
            .node =  static_cast<astref_t>(i)
        };

        symtab_put(&symtab, &key, &value);
        ASSERT_EQ(symtab.len, i + 1);

        const symrecord_t *rec {};
        ASSERT_TRUE(symtab_get(&symtab, &key, &rec));
        ASSERT_EQ(rec->node, i);

        node_ident_literal_t key2 {
            .span = srcspan_from("test1"),
            .hash = srcspan_hash(srcspan_from("test1"))
        };
        ASSERT_FALSE(symtab_get(&symtab, &key2, &rec));
    }

    symtab_free(&symtab);
}

extern "C" astref_t get_mock_var(astpool_t *pool);
extern "C" astref_t get_mock_class(astpool_t *pool);

TEST(ast, allocate_node) {
    astpool_t pool {};
    astpool_init(&pool);

    for (int i = 1; i < 0xffff; ++i) {
        astref_t ref = astnode_new_int(&pool, i == 0xffff>>1 ? 42 : 11, NULL);
        ASSERT_EQ(ref, i);
        ASSERT_NE(ref, ASTREF_NULL);
        ASSERT_FALSE(astref_isnull(ref));
        ASSERT_TRUE(astpool_isvalidref(&pool, ref));
        astnode_t *node = astpool_resolve(&pool, ref);
        ASSERT_EQ(node->type, ASTNODE_INT_LIT);
        ASSERT_EQ(node->dat.n_int_lit.value, i == 0xffff>>1 ? 42 : 11);
    }

    ASSERT_EQ(pool.node_pool.len, sizeof(astnode_t)*(0xffff-1));
    astnode_t *node = neo_mempool_getelementptr(pool.node_pool, (0xffff>>1)-1, astnode_t);
    ASSERT_EQ(node->dat.n_int_lit.value, 42);
    node = neo_mempool_getelementptr(pool.node_pool, 22, astnode_t);
    ASSERT_EQ(node->dat.n_int_lit.value, 11);

    astref_t ref = ASTREF_NULL;
    ASSERT_TRUE(astref_isnull(ref));
    ASSERT_FALSE(astpool_isvalidref(&pool, ref));
    node = astpool_resolve(&pool, ref);
    ASSERT_EQ(node, nullptr);

    astpool_free(&pool);
}

TEST(ast, allocate_node2) {
    astpool_t pool {};
    astpool_init(&pool);

    astref_t ref = astnode_new_int(&pool, 3, NULL);
    ASSERT_EQ(ref, 1);
    ASSERT_EQ(pool.node_pool.len, sizeof(astnode_t));

    astpool_free(&pool);
}

#if 0
TEST(ast, block_push_children) {
    astpool_t pool {};
    astpool_init(&pool);

    node_block_t block {
        .blktype = BLOCKSCOPE_LOCAL
    };
    ASSERT_EQ(block.len, 0);
    //ASSERT_EQ(block.nodes, nullptr);

    astref_t var = get_mock_var(&pool);

    node_block_push_child(&pool, &block, var);
    ASSERT_EQ(block.len, 1);
    //ASSERT_NE(block.nodes, nullptr);
    ASSERT_NE(block.cap, 0);
    ASSERT_EQ(block.nodes[0], var);
    ASSERT_TRUE(astpool_resolve(&pool, block.nodes[0])->type == ASTNODE_VARIABLE);
    
    for (int i {}; i < 512; ++i) {
        node_block_push_child(&pool, &block, var);
    }

    ASSERT_EQ(block.len, 1+512);
    ASSERT_NE(block.nodes, nullptr);
    ASSERT_NE(block.cap, 0);
    for (int i {}; i < block.len; ++i) {
        ASSERT_EQ(block.nodes[i], var);
    }

    astref_t noderef = astnode_new_block(&pool, &block);
    astnode_t *node = astpool_resolve(&pool, noderef);
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->dat.n_block.len, 1+512);
    ASSERT_NE(node->dat.n_block.nodes, nullptr);
    ASSERT_NE(node->dat.n_block.cap, 0);

    astpool_free(&pool);
}
#endif

TEST(ast, int_literal) {
    astpool_t mempool {};
    astpool_init(&mempool);

    astref_t ref = astnode_new_int(&mempool, 42, NULL);
    astnode_t *node = astpool_resolve(&mempool, ref);
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->type, ASTNODE_INT_LIT);
    ASSERT_EQ(node->dat.n_int_lit.value, 42);

    astpool_free(&mempool);
}

TEST(ast, unary_op) {
    astpool_t mempool {};
    astpool_init(&mempool);

    astref_t operand_ode = astnode_new_int(&mempool, 10, NULL);
    node_unary_op_t unary_op_node_data;
    unary_op_node_data.opcode = UNOP_MINUS;
    unary_op_node_data.child_expr = operand_ode;
    astref_t unary_op_ode_ref = astnode_new_unary_op(&mempool, &unary_op_node_data);
    const astnode_t *unary_op_ode = astpool_resolve(&mempool, unary_op_ode_ref);
    ASSERT_NE(unary_op_ode, nullptr);
    ASSERT_EQ(unary_op_ode->type, ASTNODE_UNARY_OP);
    ASSERT_EQ(unary_op_ode->dat.n_unary_op.opcode, UNOP_MINUS);
    ASSERT_EQ(unary_op_ode->dat.n_unary_op.child_expr, operand_ode);

    astpool_free(&mempool);
}

TEST(ast, group) {
    astpool_t mempool {};
    astpool_init(&mempool);

    node_group_t groupData;
    astref_t childNode = astnode_new_int(&mempool, 42, NULL);
    groupData.child_expr = childNode;
    astref_t groupNodeRef = astnode_new_group(&mempool, &groupData);
    const astnode_t *groupNode = astpool_resolve(&mempool, groupNodeRef);
    ASSERT_NE(groupNode, nullptr);
    ASSERT_EQ(groupNode->type, ASTNODE_GROUP);
    ASSERT_EQ(groupNode->dat.n_group.child_expr, childNode);

    astpool_free(&mempool);
}

TEST(ast, visit) {
    astpool_t mempool {};
    astpool_init(&mempool);

    astref_t mock = get_mock_class(&mempool);

    static std::unordered_map<astnode_type_t, int> node_type_counts {};

    static astref_t *mmock;
    mmock = &mock;
    static int count {};
    auto visitor = [](astpool_t *pool, astref_t node, void *data) -> void {
        ASSERT_TRUE(astpool_isvalidref(pool, node));
        astnode_t *pnode = astpool_resolve(pool, node);
        ++node_type_counts[pnode->type];
        ASSERT_NE(pnode, nullptr);
        ASSERT_EQ(data, mmock);
        ++count;
    };
    astnode_visit(&mempool, mock, visitor, &mock);
    ASSERT_EQ(count, 19);

    for (const auto& [k, v] : node_type_counts) {
        std::cout << astnode_names[k] << " : " << v << "\n";
    }

    astpool_free(&mempool);
}
