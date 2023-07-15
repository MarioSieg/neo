// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <format>
#include <filesystem>
#include <memory>

namespace neoc {
    class lex_char32 final {
    public:
        constexpr lex_char32() noexcept : c_{U'\0'} {}
        explicit constexpr lex_char32(char32_t c) noexcept : c_ {c} {}
        explicit constexpr lex_char32(char c) noexcept : c_ {static_cast<char32_t>(c)} {}
        constexpr lex_char32(const lex_char32&) noexcept = default;
        constexpr lex_char32(lex_char32&&) noexcept = default;
        constexpr auto operator = (const lex_char32&) noexcept -> lex_char32& = default;
        constexpr auto operator = (lex_char32&&) noexcept -> lex_char32& = default;
        ~lex_char32() noexcept = default;
        explicit constexpr operator char32_t() const noexcept { return c_; }
        explicit constexpr operator std::uint32_t() const noexcept { return static_cast<std::uint32_t>(c_); }
        explicit constexpr operator char() const noexcept { return static_cast<char>(c_); }
        constexpr auto operator * () const noexcept -> char32_t { return c_; }
        constexpr auto operator = (char32_t c) noexcept -> lex_char32& { c_ = c; return *this; }
        constexpr auto operator = (lex_char32 c) noexcept -> lex_char32& { *this = c.c_; return *this; }
        constexpr auto operator == (char32_t c) const noexcept -> bool { return c_ == c; }
        constexpr auto operator != (char32_t c) const noexcept -> bool { return !(*this == c); }
        constexpr auto operator == (lex_char32 c) const noexcept -> bool { return c_ == c.c_; }
        constexpr auto operator != (lex_char32 c) const noexcept -> bool { return !(*this == c.c_); }

        [[nodiscard]] constexpr auto is_within_interval(char32_t begin, char32_t end) const noexcept -> bool {
            return c_ >= begin && c_ <= end;
        }
        [[nodiscard]] constexpr auto is_ascii() const noexcept -> bool {
            return c_ < U'\x80';
        }
        [[nodiscard]] constexpr auto is_ascii_whitespace() const noexcept -> bool {
            return c_ == U' ' || c_ == U'\t' || c_ == U'\r' || c_ == U'\v' || c_ == U'\f'; // \n is not whitespace but a token in NEO
        }
        [[nodiscard]] constexpr auto is_ascii_digit() const noexcept -> bool {
            return is_within_interval(U'0', U'9');
        }
        [[nodiscard]] constexpr auto is_ascii_hex_digit() const noexcept -> bool {
            return is_ascii_digit() || is_within_interval(U'a', U'f') || is_within_interval(U'A', U'F');
        }
        [[nodiscard]] constexpr auto is_ascii_binary_digit() const noexcept -> bool {
            return c_ == U'0' || c_ == U'1';
        }
        [[nodiscard]] constexpr auto is_ascii_alpha() const noexcept -> bool {
            return is_within_interval(U'a', U'z') || is_within_interval(U'A', U'Z');
        }
        [[nodiscard]] constexpr auto is_ascii_alphanumeric() const noexcept -> bool {
            return is_ascii_digit() || is_ascii_alpha();
        }
        [[nodiscard]] constexpr auto ascii_to_upper() const noexcept -> char32_t {
            return is_ascii_alpha() ? c_ & ~U' ' : c_;
        }
        [[nodiscard]] constexpr auto ascii_to_lower() const noexcept -> char32_t {
            return is_ascii_alpha() ? c_ | U' ' : c_;
        }
        [[nodiscard]] constexpr auto is_whitespace() const noexcept -> bool {
            return
                is_ascii_whitespace()
                || c_ == U'\x0085'  // NEXT LINE from latin1
                || c_ == U'\x200E'  // LEFT-TO-RIGHT BIDI MARK
                || c_ == U'\x200F'  // RIGHT-TO-LEFT BIDI MARK
                || c_ == U'\x2028'  // LINE SEPARATOR
                || c_ == U'\x2029'; // PARAGRAPH SEPARATOR
        }
        [[nodiscard]] inline auto is_ident_start() const noexcept -> bool { // Checks if the character is a valid identifier start
            return is_ascii_alpha() || c_ == U'_';
        }
        [[nodiscard]] inline auto is_ident_continue() const noexcept -> bool { // Checks if the character is a valid identifier continuation
            return is_ascii_alphanumeric() || c_ == U'_';
        }
        [[nodiscard]] inline auto to_string() const -> std::string {
            return is_ascii() ? std::format("{}", static_cast<char>(*this)) : std::format("U+{:04X}", static_cast<std::uint32_t>(*this));
        }

    private:
        char32_t c_ {};
    };

    enum class radix : std::uint8_t {
        dec = 10, // Literal starts with "0b"
        hex = 16, // Literal starts with "0x"
        bin = 2, // Literal doesn't contain a prefix
        oct = 8 // Literal starts with "0c"
    };

    class source_code final {
    public:
        source_code(std::u8string&& src, std::filesystem::path&& path);
        explicit source_code(std::filesystem::path&& path);

        [[nodiscard]] inline auto get_source_code() const noexcept -> const std::u8string& { return src_; }
        [[nodiscard]] inline auto get_source_file() const noexcept -> const std::filesystem::path& { return path_; }

    private:
        [[nodiscard]] static auto load_source_from_file(const std::filesystem::path& path) -> std::u8string;
        const std::u8string src_;
        const std::filesystem::path path_;
    };

    class cursor final {
    public:
        [[nodiscard]] static constexpr auto utf8_seq_length(char8_t x) noexcept -> std::uint32_t {
            if (x == u'\0') [[unlikely]] { return 0; }
            else if (x < 0x80) [[likely]] { return 1; } // ASCII
            else if ((x >> 5) == 0x6) { return 2; }
            else if ((x >> 4) == 0xe) { return 3; }
            else if ((x >> 3) == 0x1e) { return 4; }
            else { return 0; }
        }
        [[nodiscard]] static auto utf8_iter_next(const char8_t*& p) noexcept -> char32_t;

        [[nodiscard]] inline auto is_done() const noexcept -> bool { return !src_ || !*needle_; }
        [[nodiscard]] inline auto peek() const -> lex_char32 { return lex_char32{curr_}; }
        [[nodiscard]] inline auto peek_next() const -> lex_char32 { return lex_char32{next_}; }
        auto consume() -> void;
        auto is_match(char32_t c) -> bool;
        auto set_source(const std::shared_ptr<source_code>& src) -> void;
        [[nodiscard]] inline auto get_needle() const noexcept -> const char8_t* { return needle_; }

    private:
        std::shared_ptr<source_code> src_{};
        const char8_t* src_ptr_{};
        const char8_t* needle_{};
        const char8_t* tok_start_{};
        const char8_t* line_start_{};
        char32_t curr_{}; // cached for peek()
        char32_t next_{}; // cached for peek_next()
        std::uint32_t line_{};
        std::uint32_t column_{};
    };

    class token final {
    public:
        enum type : std::uint8_t {
            // keywords
            kw_method = 0, kw_let, kw_new,
            kw_end, kw_then, kw_if, kw_else,
            kw_return, kw_class, kw_module, kw_break,
            kw_continue, kw_while, kw_static, kw_do, kw_as,

            // literals
            li_ident, li_int, li_float, li_string, li_true, li_false,

            // punctuation
            pu_l_paren, pu_r_paren, pu_l_bracket, pu_r_bracket,
            pu_l_brace, pu_r_brace, pu_comma, pu_arrow, pu_nl,

            // operators
            op_dot, op_assign, op_add, op_sub, op_mul, op_add_no_ov, op_sub_no_ov,
            op_mul_no_ov, op_div, op_mod, op_pow, op_add_assign, op_sub_assign, op_mul_assign,
            op_add_no_ov_assign, op_sub_no_ov_assign, op_mul_no_ov_assign, op_div_assign, op_mod_assign,
            op_pow_assign, op_inc, op_dec, op_equal, op_not_equal, op_less, op_less_equal, op_greater,
            op_greater_equal, op_bit_and, op_bit_or, op_bit_xor, op_bit_ashl,
            op_bit_ashr, op_bit_lshr, op_bit_and_assign, op_bit_or_assign, op_bit_xor_assign,
            op_bit_ashl_assign, op_bit_ashr_assign, op_bit_lshr_assign, op_bit_compl, op_log_and,
            op_log_or, op_log_not,

            // meta
             me_err, me_eof,

            $count
        };

        static constexpr std::array<std::u8string_view, $count> lexemes {
            // keywords
            u8"method", u8"let", u8"new", u8"end",
            u8"then", u8"if", u8"else", u8"return",
            u8"class", u8"module", u8"break", u8"continue",
            u8"while", u8"static", u8"do", u8"as",

            // literals
            u8"<ident>", u8"<int>", u8"<float>",
            u8"<string>", u8"true", u8"false",

            // punctuation
            u8"(", u8")", u8"[", u8"]", u8"{", u8"}", u8",", u8"->", u8"\\n",

            // operators
            u8".", u8"=", u8"+", u8"-", u8"*", u8"!+",
            u8"!-", u8"!*", u8"/", u8"%", u8"**", u8"+=",
            u8"-=", u8"*=", u8"!+=", u8"!-=", u8"!*=",
            u8"/=", u8"%=", u8"**=", u8"++", u8"--",
            u8"==", u8"!=", u8"<", u8"<=", u8">", u8">=",
            u8"&", u8"|", u8"^", u8"<<", u8">>",
            u8">>>", u8"&=", u8"|=", u8"^=", u8"<<=",
            u8">>=", u8">>>=", u8"~",
            u8"and", u8"or", u8"not",

            // meta
            u8"<error>",
            u8"<eof>"
        };
        static constexpr std::array<std::underlying_type_t<type>, 2> keyword_range { kw_method, kw_as }; // must be updated when keywords are added
        static_assert(keyword_range[0] == 0 && keyword_range[1] == 15 && keyword_range[1] >= keyword_range[0]);

        [[nodiscard]] inline auto get_type() const noexcept -> type { return type_; }
        [[nodiscard]] inline auto get_type_name() const noexcept -> std::u8string_view { return lexemes[type_]; }
        [[nodiscard]] inline auto get_type_name_ascii() const noexcept -> std::string_view {
            auto utf8{get_type_name()};
            return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
        }
        [[nodiscard]] inline auto get_radix() const noexcept -> radix { return radix_; }
        [[nodiscard]] inline auto get_lexeme() const noexcept -> std::u8string_view { return lexeme_; }
        [[nodiscard]] inline auto get_lexeme_line() const noexcept -> std::u8string_view { return lexeme_line; }
        [[nodiscard]] inline auto get_line() const noexcept -> std::uint32_t { return line_; }
        [[nodiscard]] inline auto get_column() const noexcept -> std::uint32_t { return column_; }

    private:
        type type_ {me_eof};
        radix radix_ {radix::dec}; // only used if type_ == li_int
        std::u8string_view lexeme_ {};
        std::u8string_view lexeme_line {};
        std::uint32_t line_ {};
        std::uint32_t column_ {};
    };

    class lex_exception final : public std::runtime_error {
    public:
        using runtime_error::runtime_error;
    };
}
