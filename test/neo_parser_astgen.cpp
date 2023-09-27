/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neo_compiler.hpp>
#include <neo_parser.h>

// helper for faster checks
struct astref final {
    constexpr astref(astref_t r, astpool_t &p) : r{r}, p{&p} {}
    astref_t r;
    astpool_t *p;
    inline operator bool() const { return !astref_isnull(r); }
    inline astnode_t *operator -> () const {
        if (astref_isnull(r)) {
            throw std::runtime_error("astref is null");
        }
        return astpool_resolve(p, r);
    }
    inline astnode_type_t operator * () const {
        auto ty =  (*this)->type;
        if (ty == ASTNODE_ERROR) {
            std::string msg {"astref is error: " + std::string((*this)->dat.n_error.message)};
            throw std::runtime_error(msg);
        }
        return ty;
    }
    inline bool operator == (astnode_type_t t) const { return **this == t; }
    inline bool operator != (astnode_type_t t) const { return **this != t; }
};

#define ast_gen_test(name, src, body) \
TEST(parser, astgen_##name) { \
    error_vector_t ev {}; \
    errvec_init(&ev); \
    parser_t parser; \
    parser_init(&parser, &ev); \
    parser_setup_source(&parser, source_from_memory_ref((const uint8_t *)#name, (const uint8_t *)src, NULL)); \
     \
    astref_t root {parser_drain(&parser)}; \
    ASSERT_FALSE(astref_isnull(root)); \
    errvec_print(&ev, stdout, true); \
    ASSERT_TRUE(errvec_isempty(ev)); \
     body \
    parser_free(&parser); \
    errvec_free(&ev); }

ast_gen_test(rule_variable,
"let x: int = (10 + yy) * 3\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_VARIABLE);
    astref ident (stmt->dat.n_variable.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("x")));
    astref type (stmt->dat.n_variable.type, parser.pool);
    ASSERT_EQ(type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(type->dat.n_ident_lit.span, srcspan_from("int")));
    astref expr (stmt->dat.n_variable.init_expr, parser.pool);
    ASSERT_EQ(expr, ASTNODE_BINARY_OP);
    ASSERT_EQ(expr->dat.n_binary_op.opcode, BINOP_MUL);
    astref left (expr->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_GROUP);
    left = astref(left->dat.n_group.child_expr, parser.pool);
    ASSERT_EQ(left->dat.n_binary_op.opcode, BINOP_ADD);
    astref right (expr->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_INT_LIT);
    ASSERT_EQ(right->dat.n_int_lit.value, 3);
    astref left_left (left->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left_left, ASTNODE_INT_LIT);
    ASSERT_EQ(left_left->dat.n_int_lit.value, 10);
    astref left_right (left->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(left_right, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left_right->dat.n_ident_lit.span, srcspan_from("yy")));
})

ast_gen_test(
rule_function_plain,
"func f()\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_FUNCTION);
    astref ident (stmt->dat.n_method.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("f")));
    astref params (stmt->dat.n_method.params, parser.pool);
    ASSERT_FALSE(params); // no params
    astref ret_type (stmt->dat.n_method.ret_type, parser.pool);
    ASSERT_FALSE(ret_type); // no return type
    astref body2 (stmt->dat.n_method.body, parser.pool);
    ASSERT_FALSE(body2); // no body
})

ast_gen_test(
rule_function_plain_return_type,
"func f() -> int\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_FUNCTION);
    astref ident (stmt->dat.n_method.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("f")));
    astref params (stmt->dat.n_method.params, parser.pool);
    ASSERT_FALSE(params); // no params
    astref ret_type (stmt->dat.n_method.ret_type, parser.pool);
    ASSERT_EQ(ret_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ret_type->dat.n_ident_lit.span, srcspan_from("int")));
    astref body2 (stmt->dat.n_method.body, parser.pool);
    ASSERT_FALSE(body2); // no body
})

ast_gen_test(
rule_function_one_param,
"func f(x: int)\n end\n",
{
astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_FUNCTION);
    astref ident (stmt->dat.n_method.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("f")));
    astref params (stmt->dat.n_method.params, parser.pool);
    ASSERT_EQ(params, ASTNODE_BLOCK);
    ASSERT_EQ(params->dat.n_block.len, 1);
    ASSERT_EQ(params->dat.n_block.scope, BLOCKSCOPE_PARAMLIST);
    astref param (astpool_resolvelist(&parser.pool, params->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_VARIABLE);
    astref param_ident (param->dat.n_variable.ident, parser.pool);
    ASSERT_EQ(param_ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_ident->dat.n_ident_lit.span, srcspan_from("x")));
    astref param_type (param->dat.n_variable.type, parser.pool);
    ASSERT_EQ(param_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_type->dat.n_ident_lit.span, srcspan_from("int")));
    astref ret_type (stmt->dat.n_method.ret_type, parser.pool);
    ASSERT_FALSE(ret_type); // no return type
    astref body2 (stmt->dat.n_method.body, parser.pool);
    ASSERT_FALSE(body2); // no body
})

ast_gen_test(
rule_function_two_params,
"func f(x: int, y: float)\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_FUNCTION);
    astref ident (stmt->dat.n_method.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("f")));
    astref params (stmt->dat.n_method.params, parser.pool);
    ASSERT_EQ(params, ASTNODE_BLOCK);
    ASSERT_EQ(params->dat.n_block.len, 2);
    ASSERT_EQ(params->dat.n_block.scope, BLOCKSCOPE_PARAMLIST);
    astref param (astpool_resolvelist(&parser.pool, params->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_VARIABLE);
    astref param_ident (param->dat.n_variable.ident, parser.pool);
    ASSERT_EQ(param_ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_ident->dat.n_ident_lit.span, srcspan_from("x")));
    astref param_type (param->dat.n_variable.type, parser.pool);
    ASSERT_EQ(param_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_type->dat.n_ident_lit.span, srcspan_from("int")));
    param = astref(astpool_resolvelist(&parser.pool, params->dat.n_block.nodes)[1], parser.pool);
    ASSERT_EQ(param, ASTNODE_VARIABLE);
    param_ident = astref(param->dat.n_variable.ident, parser.pool);
    ASSERT_EQ(param_ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_ident->dat.n_ident_lit.span, srcspan_from("y")));
    param_type = astref(param->dat.n_variable.type, parser.pool);
    ASSERT_EQ(param_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_type->dat.n_ident_lit.span, srcspan_from("float")));
    astref ret_type (stmt->dat.n_method.ret_type, parser.pool);
    ASSERT_FALSE(ret_type); // no return type
    astref body2 (stmt->dat.n_method.body, parser.pool);
    ASSERT_FALSE(body2); // no body
})

ast_gen_test(
rule_function_param_and_return_type,
"func f(x: int) -> float\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_FUNCTION);
    astref ident (stmt->dat.n_method.ident, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("f")));
    astref params (stmt->dat.n_method.params, parser.pool);
    ASSERT_EQ(params, ASTNODE_BLOCK);
    ASSERT_EQ(params->dat.n_block.len, 1);
    ASSERT_EQ(params->dat.n_block.scope, BLOCKSCOPE_PARAMLIST);
    astref param (astpool_resolvelist(&parser.pool, params->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_VARIABLE);
    astref param_ident (param->dat.n_variable.ident, parser.pool);
    ASSERT_EQ(param_ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_ident->dat.n_ident_lit.span, srcspan_from("x")));
    astref param_type (param->dat.n_variable.type, parser.pool);
    ASSERT_EQ(param_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_type->dat.n_ident_lit.span, srcspan_from("int")));
    astref ret_type (stmt->dat.n_method.ret_type, parser.pool);
    ASSERT_EQ(ret_type, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ret_type->dat.n_ident_lit.span, srcspan_from("float")));
    astref body2 (stmt->dat.n_method.body, parser.pool);
    ASSERT_FALSE(body2); // no body
})

ast_gen_test(
rule_branch,
"if 10 != 2 or y != k then exit()\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BRANCH);
    astref cond (stmt->dat.n_branch.cond_expr, parser.pool);
    ASSERT_EQ(cond, ASTNODE_BINARY_OP);
    ASSERT_EQ(cond->dat.n_binary_op.opcode, BINOP_LOG_OR);
    astref left (cond->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_BINARY_OP);
    ASSERT_EQ(left->dat.n_binary_op.opcode, BINOP_NOT_EQUAL);
    astref right (cond->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BINARY_OP);
    ASSERT_EQ(right->dat.n_binary_op.opcode, BINOP_NOT_EQUAL);
    astref left_left (left->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left_left, ASTNODE_INT_LIT);
    ASSERT_EQ(left_left->dat.n_int_lit.value, 10);
    astref left_right (left->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(left_right, ASTNODE_INT_LIT);
    ASSERT_EQ(left_right->dat.n_int_lit.value, 2);
    astref right_left (right->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(right_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_left->dat.n_ident_lit.span, srcspan_from("y")));
    astref right_right (right->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right_right, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_right->dat.n_ident_lit.span, srcspan_from("k")));
    astref body2 (stmt->dat.n_branch.true_block, parser.pool);
    ASSERT_EQ(stmt->dat.n_branch.false_block, ASTREF_NULL);
    ASSERT_EQ(body2, ASTNODE_BLOCK);
    ASSERT_EQ(body2->dat.n_block.len, 1);
    astref stmt2 (astpool_resolvelist(&parser.pool, body2->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt2, ASTNODE_BINARY_OP);
    astref ident (stmt2->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("exit")));
})

ast_gen_test(
rule_while_loop,
"while x and y > k do sub()\n end\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_LOOP);
    astref cond (stmt->dat.n_loop.cond_expr, parser.pool);
    ASSERT_EQ(cond, ASTNODE_BINARY_OP);
    ASSERT_EQ(cond->dat.n_binary_op.opcode, BINOP_LOG_AND);
    astref left (cond->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("x")));
    astref right (cond->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BINARY_OP);
    ASSERT_EQ(right->dat.n_binary_op.opcode, BINOP_GREATER);
    astref right_left (right->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(right_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_left->dat.n_ident_lit.span, srcspan_from("y")));
    astref right_right (right->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right_right, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_right->dat.n_ident_lit.span, srcspan_from("k")));
    astref body2 (stmt->dat.n_loop.true_block, parser.pool);
    ASSERT_EQ(body2, ASTNODE_BLOCK);
    ASSERT_EQ(body2->dat.n_block.len, 1);
    astref stmt2 (astpool_resolvelist(&parser.pool, body2->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt2, ASTNODE_BINARY_OP);
    astref ident (stmt2->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(ident, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(ident->dat.n_ident_lit.span, srcspan_from("sub")));
})

ast_gen_test(
rule_free_expr_redundant,
"x + 10\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    astref left (stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("x")));
    astref right (stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_INT_LIT);
    ASSERT_EQ(right->dat.n_int_lit.value, 10);
})

ast_gen_test(
rule_free_expr_assign_simple,
"points *= boost + highscore * 10\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    ASSERT_EQ(stmt->dat.n_binary_op.opcode, BINOP_MUL_ASSIGN);
    astref left (stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("points")));
    astref right (stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BINARY_OP);
    ASSERT_EQ(right->dat.n_binary_op.opcode, BINOP_ADD);
    astref right_left (right->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(right_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_left->dat.n_ident_lit.span, srcspan_from("boost")));
    astref right_right (right->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right_right, ASTNODE_BINARY_OP);
    ASSERT_EQ(right_right->dat.n_binary_op.opcode, BINOP_MUL);
    astref right_right_left (right_right->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(right_right_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(right_right_left->dat.n_ident_lit.span, srcspan_from("highscore")));
    astref right_right_right (right_right->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right_right_right, ASTNODE_INT_LIT);
    ASSERT_EQ(right_right_right->dat.n_int_lit.value, 10);
})

ast_gen_test(
rule_free_expr_function_call_no_params,
"exit()\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    ASSERT_EQ(stmt->dat.n_binary_op.opcode, BINOP_CALL);
    astref left (stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("exit")));
    astref right (stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTREF_NULL);
})

ast_gen_test(
rule_free_expr_function_call_one_param,
"exit(0)\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    astref body (mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    astref stmt (astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    ASSERT_EQ(stmt->dat.n_binary_op.opcode, BINOP_CALL);
    astref left (stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("exit")));
    astref right (stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BLOCK);
    ASSERT_EQ(right->dat.n_block.len, 1);
    ASSERT_EQ(right->dat.n_block.scope, BLOCKSCOPE_ARGLIST);
    astref param (astpool_resolvelist(&parser.pool, right->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_INT_LIT);
    ASSERT_EQ(param->dat.n_int_lit.value, 0);
})

ast_gen_test(
rule_free_expr_function_call_two_params,
"exit(0, msg)\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    auto body = astref(mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    auto stmt = astref(astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    ASSERT_EQ(stmt->dat.n_binary_op.opcode, BINOP_CALL);
    auto left = astref(stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("exit")));
    auto right = astref(stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BLOCK);
    ASSERT_EQ(right->dat.n_block.len, 2);
    ASSERT_EQ(right->dat.n_block.scope, BLOCKSCOPE_ARGLIST);
    auto param = astref(astpool_resolvelist(&parser.pool, right->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_INT_LIT);
    ASSERT_EQ(param->dat.n_int_lit.value, 0);
    param = astref(astpool_resolvelist(&parser.pool, right->dat.n_block.nodes)[1], parser.pool);
    ASSERT_EQ(param, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param->dat.n_string_lit.span, srcspan_from("msg")));
})

ast_gen_test(
rule_free_expr_function_call_two_params_expr,
"calc(x+1, (x**2)>>1)\n",
{
    astref mod (root, parser.pool);
    ASSERT_EQ(mod, ASTNODE_MODULE);
    ASSERT_EQ(mod->dat.n_module.ident, ASTREF_NULL);
    auto body = astref(mod->dat.n_module.body, parser.pool);
    ASSERT_EQ(body, ASTNODE_BLOCK);
    ASSERT_EQ(body->dat.n_block.len, 1);
    auto stmt = astref(astpool_resolvelist(&parser.pool, body->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(stmt, ASTNODE_BINARY_OP);
    ASSERT_EQ(stmt->dat.n_binary_op.opcode, BINOP_CALL);
    auto left = astref(stmt->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, srcspan_from("calc")));
    auto right = astref(stmt->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(right, ASTNODE_BLOCK);
    ASSERT_EQ(right->dat.n_block.len, 2);
    ASSERT_EQ(right->dat.n_block.scope, BLOCKSCOPE_ARGLIST);
    auto param = astref(astpool_resolvelist(&parser.pool, right->dat.n_block.nodes)[0], parser.pool);
    ASSERT_EQ(param, ASTNODE_BINARY_OP);
    ASSERT_EQ(param->dat.n_binary_op.opcode, BINOP_ADD);
    auto param_left = astref(param->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(param_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_left->dat.n_ident_lit.span, srcspan_from("x")));
    auto param_right = astref(param->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(param_right, ASTNODE_INT_LIT);
    ASSERT_EQ(param_right->dat.n_int_lit.value, 1);
    param = astref(astpool_resolvelist(&parser.pool, right->dat.n_block.nodes)[1], parser.pool);
    ASSERT_EQ(param, ASTNODE_BINARY_OP);
    ASSERT_EQ(param->dat.n_binary_op.opcode, BINOP_BIT_ASHR);
    param_left = astref(param->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(*param_left, ASTNODE_GROUP);
    param_left = astref(param_left->dat.n_group.child_expr, parser.pool);
    ASSERT_EQ(param_left, ASTNODE_BINARY_OP);
    ASSERT_EQ(param_left->dat.n_binary_op.opcode, BINOP_POW);
    param_right = astref(param->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(param_right, ASTNODE_INT_LIT);
    ASSERT_EQ(param_right->dat.n_int_lit.value, 1);
    param_left = astref(param_left->dat.n_binary_op.left_expr, parser.pool);
    ASSERT_EQ(param_left, ASTNODE_IDENT_LIT);
    ASSERT_TRUE(srcspan_eq(param_left->dat.n_ident_lit.span, srcspan_from("x")));
    param_right = astref(param_left->dat.n_binary_op.right_expr, parser.pool);
    ASSERT_EQ(param_right, ASTNODE_INT_LIT);
    ASSERT_EQ(param_right->dat.n_int_lit.value, 2);
})
