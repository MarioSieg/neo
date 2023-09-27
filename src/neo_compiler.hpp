// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com
// High-level C++ RAII wrapper for the NEO compiler API for embedding/hosting etc.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "neo_compiler.h"

namespace neo {
    class source_code final {
    public:
        inline explicit source_code(const std::uint8_t *filename) {
            source_load_error_info_t info {};
            m_src = source_from_file(filename, &info);
            if (neo_unlikely(!m_src || info.error != SRCLOAD_OK)) {
                const char *msg;
                switch (info.error) {
                    case SRCLOAD_INVALID_UTF8: msg = "Invalid UTF-8"; break;
                    case SRCLOAD_FILE_NOT_FOUND: msg = "File not found"; break;
                    case SRCLOAD_FILE_READ_ERROR: msg = "File read error"; break;
                    default: msg = "Unknown error"; break;
                }
                throw std::runtime_error("Failed to load source file: " + std::string {msg});
            }
        }
        inline explicit source_code(const std::string &filename)
            : source_code {reinterpret_cast<const std::uint8_t *>(filename.c_str())} {}
        inline explicit source_code(const std::uint8_t *filename, const std::uint8_t *src) {
            source_load_error_info_t info {};
            m_src = source_from_memory_ref(filename, src, &info);
            if (neo_unlikely(!m_src || info.error != SRCLOAD_OK)) {
                const char *msg;
                switch (info.error) {
                    case SRCLOAD_INVALID_UTF8: msg = "Invalid UTF-8"; break;
                    case SRCLOAD_FILE_NOT_FOUND: msg = "File not found"; break;
                    case SRCLOAD_FILE_READ_ERROR: msg = "File read error"; break;
                    default: msg = "Unknown error"; break;
                }
                throw std::runtime_error("Failed to load source file: " + std::string {msg});
            }
        }
        inline explicit source_code(const std::string &filename, const std::string &src)
            : source_code {reinterpret_cast<const std::uint8_t *>(filename.c_str()), reinterpret_cast<const std::uint8_t *>(src.c_str())} {}

        source_code(const source_code &) = delete; /* No copy. */
        inline source_code(source_code && other) noexcept {
            m_src = other.m_src;
            other.m_src = nullptr;
        }
        source_code &operator = (const source_code &) = delete; /* No move. */
        source_code &operator = (source_code && other) noexcept {
            if (this == &other) { return *this; }
            if (m_src) { source_free(m_src); }
            m_src = other.m_src;
            other.m_src = nullptr;
            return *this;
        }
        inline ~source_code() {
            if (m_src) { source_free(m_src); }
            m_src = nullptr;
        }
        inline const uint8_t *get_file_name() const noexcept {
            return m_src->filename;
        }
        inline const uint8_t *get_source() const noexcept {
            return m_src->src;
        }
        inline std::uint32_t get_source_length() const noexcept {
            return m_src->len;
        }
        inline const source_t *operator * () const noexcept {
            return m_src;
        }

    private:
        const source_t *m_src {};
    };

    class compiler final {
    public:
        inline explicit compiler(neo_compiler_flag_t flags = neo_compiler_flag_t::COM_FLAG_NONE) {
            compiler_init(&m_compiler, flags);
        }
        compiler(const compiler &) = delete; /* No copy. */
        compiler(compiler &&) = delete; /* No copy. */
        compiler &operator = (const compiler &) = delete; /* No move. */
        compiler &operator = (compiler &&) = delete; /* No move. */
        inline ~compiler() {
            compiler_free(&m_compiler);
            m_compiler = nullptr;
        }
        inline bool operator ()(const source_code &src, void *usr = nullptr) {
            return compiler_compile(m_compiler, *src, usr);
        }
        inline const error_vector_t &get_errors() const noexcept {
            return *compiler_get_errors(m_compiler);
        }
        inline std::vector<const compile_error_t *> get_cloned_error_vec() const {
            const auto &errors {get_errors()};
            std::vector<const compile_error_t *> result {};
            result.reserve(errors.len);
            for (std::uint32_t i {}; i < errors.len; ++i) {
                result.emplace_back(errors.p[i]);
            }
            return result;
        }
        inline const astref_t get_ast_root(const astpool_t *&pool) const noexcept {
            return compiler_get_ast_root(m_compiler, &pool);
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
        inline compiler &operator |= (neo_compiler_flag_t flags) noexcept {
            set_flags(static_cast<neo_compiler_flag_t>(get_flags() | flags));
            return *this;
        }
        inline compiler &operator &= (neo_compiler_flag_t flags) noexcept {
            set_flags(static_cast<neo_compiler_flag_t>(get_flags() & flags));
            return *this;
        }

    private:
        neo_compiler_t *m_compiler {};
    };
}
