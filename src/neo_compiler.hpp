// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com
// High-level C++ RAII wrapper for the NEO compiler API for embedding/hosting etc.

#pragma once

#include <neo_compiler.h>

namespace neo {
    /*
    ** Minimal C++ RAII wrapper around the high-level NEO compiler C-API.
    */
    class neo_compiler final {
    public:
        inline explicit neo_compiler(neo_compiler_flag_t flags = neo_compiler_flag_t::COM_FLAG_NONE) {
            compiler_init(&m_compiler, flags);
        }
        neo_compiler(const neo_compiler &) = delete; /* No copy. */
        neo_compiler(neo_compiler &&) = delete; /* No copy. */
        neo_compiler &operator = (const neo_compiler &) = delete; /* No move. */
        neo_compiler &operator = (neo_compiler &&) = delete; /* No move. */
        inline ~neo_compiler() {
            compiler_free(&m_compiler);
            m_compiler = nullptr;
        }
        inline const error_list_t &get_errors() const noexcept {
            return *compiler_get_errors(m_compiler);
        }
        inline const astnode_t *get_ast_root() const noexcept {
            return compiler_get_ast_root(m_compiler);
        }
        inline neo_compiler_flag_t get_flags() const noexcept {
            return compiler_get_flags(m_compiler);
        }
        inline bool has_flags(neo_compiler_flag_t flags) const noexcept {
            return compiler_has_flags(m_compiler, flags);
        }
        inline neo_compile_callback_hook_t *get_pre_compile_callback() const noexcept {
            return compiler_get_pre_compile_callback(m_compiler);
        }
        inline neo_compile_callback_hook_t *get_post_compile_callback() const noexcept {
            return compiler_get_post_compile_callback(m_compiler);
        }
        inline neo_compile_callback_hook_t *get_on_warning_callback() const noexcept {
            return compiler_get_on_warning_callback(m_compiler);
        }
        inline neo_compile_callback_hook_t *get_on_error_callback() const noexcept {
            return compiler_get_on_error_callback(m_compiler);
        }
        inline void set_flags(neo_compiler_flag_t flag) noexcept {
            compiler_set_flags(m_compiler, flag);
        }
        inline void add_flag(neo_compiler_flag_t flag) noexcept {
            compiler_add_flag(m_compiler, flag);
        }
        inline void remove_flag(neo_compiler_flag_t flag) noexcept {
            compiler_remove_flag(m_compiler, flag);
        }
        inline void toggle_flag(neo_compiler_flag_t flag) noexcept {
            compiler_toggle_flag(m_compiler, flag);
        }
        inline void set_pre_compile_callback(neo_compile_callback_hook_t *new_callback) noexcept {
            compiler_set_pre_compile_callback(m_compiler, new_callback);
        }
        inline void set_post_compile_callback(neo_compile_callback_hook_t *new_callback) noexcept {
            compiler_set_post_compile_callback(m_compiler, new_callback);
        }
        inline void set_on_warning_callback(neo_compile_callback_hook_t *new_callback) noexcept {
            compiler_set_on_warning_callback(m_compiler, new_callback);
        }
        inline void set_on_error_callback(neo_compile_callback_hook_t *new_callback) noexcept {
            compiler_set_on_error_callback(m_compiler, new_callback);
        }
        inline neo_compiler_t *operator * () const noexcept {
            return m_compiler;
        }
        inline neo_compiler_t *operator -> () const noexcept {
            return m_compiler;
        }

    private:
        neo_compiler_t *m_compiler {};
    };
}
