// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include "neo_lexer.hpp"
#include "simdutf.h"

#include <fstream>

namespace neoc {
    auto source_code::load_source_from_file(const std::filesystem::path& path) -> std::u8string {
        static_assert(sizeof(char) == sizeof(char8_t));
        std::ifstream file {path, std::ios::binary};
        if (!file.is_open() || !file.good()) [[unlikely]] {
            throw lex_exception {std::format("Failed to open source file for reading - make sure the file exists: '{}'", path.string())}; // failed to open file
        }
        file.seekg(0, std::ios::end);
        std::streampos len {file.tellg()}; // determine length
        file.seekg(0);
        if (!len) [[unlikely]] { // empty file
           return {};
        }
        std::u8string result{};
        result.resize(len);
        file.read(reinterpret_cast<char*>(result.data()), len); // read contents
        simdutf::encoding_type encoding {simdutf::BOM::check_bom(reinterpret_cast<const std::uint8_t*>(result.c_str()), result.size())};
        std::size_t bom_size {simdutf::BOM::bom_byte_size(encoding)};
        result.erase(0, bom_size); // remove BOM
        if (encoding != simdutf::encoding_type::UTF8 && encoding != simdutf::encoding_type::unspecified) [[unlikely]] {
            throw lex_exception {std::format("Unsupported source file encoding - must bei either ASCII or UTF-8: '{}'", path.string())};
        }
        // nothing else to do, result will be validating in the source_code constructor
        return result;
    }

    source_code::source_code(std::u8string&& src, std::filesystem::path&& path) : src_{std::move(src)}, path_{std::move(path)} {
        simdutf::result validation_result{simdutf::validate_utf8_with_errors(reinterpret_cast<const char*>(src_.data()), src_.size())};
        if (validation_result.error != simdutf::SUCCESS) [[unlikely]] {
            throw lex_exception {std::format("Invalid UTF-8 encoding ({:#x}) at position {} in file: '{}'", static_cast<std::uint8_t>(src_[validation_result.count]), validation_result.count, path_.string())};
        }
    }

    source_code::source_code(std::filesystem::path&& path) : source_code{load_source_from_file(path), std::move(path)} {}

    auto cursor::peek() const -> lex_char32 {
        if (is_done()) [[unlikely]] { return {}; }
        const char8_t* tmp{needle_};
        char32_t r{utf8_iter_next(tmp)};
        return lex_char32{r};
    }

    auto cursor::peek_next() const -> lex_char32 {
        if (is_done()) [[unlikely]] { return {}; }
        const char8_t* tmp{needle_};
        char32_t r{utf8_iter_next(tmp)};
        if (!r) [[unlikely]] { return {}; }
        r = utf8_iter_next(tmp);
        return lex_char32{r};
    }

    auto cursor::consume() -> void {
        if (is_done()) [[unlikely]] { return; }
        const char8_t* tmp{needle_};
        if (!*tmp) [[unlikely]] { // we're done
            return;
        } else if (*tmp == '\n') {
            ++line_;
            column_ = 1;
            line_start_ = needle_ + 1;
        } else {
            ++column_;
        }
        needle_ += utf8_seq_length(*needle_);
    }

    auto cursor::is_match(char32_t c) -> bool {
        if (peek() == c) {
            consume();
            return true;
        }
        return false;
    }

    auto cursor::set_source(const std::shared_ptr<source_code>& src) -> void {
        if (!src) { throw lex_exception{"source cannot be null"}; }
        src_ = src;
        src_ptr_ = src_->get_source_code().data();
        needle_ = tok_start_ = line_start_ = src_ptr_;
        line_ = 1;
        column_ = 1;
    }
}
