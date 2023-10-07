/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neo_compiler.hpp>
#include <neo_parser.h>

#include <filesystem>
#include <vector>

[[nodiscard]] static auto load_all_source_files_from_dir(const std::string &dir) {
    std::vector<neo::source_code> files {};
    for (auto &&entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            files.emplace_back(neo::source_code{entry.path().string()});
        }
    }
    return files;
}

TEST(parser_files, accept) {
    std::vector<neo::source_code> sources {load_all_source_files_from_dir("test/files/parser/accept")};
    for (auto &&src : sources) {
        std::cout << "Parsing valid: " << src.get_file_name() << std::endl;
        error_vector_t ev;
        errvec_init(&ev);
        parser_t parser;
        parser_init(&parser, &ev);
        parser_setup_source(&parser, *src);

        astref_t root {parser_drain(&parser)};
        ASSERT_FALSE(astref_isnull(root));
        errvec_print(&ev, stdout, true);
        ASSERT_TRUE(errvec_isempty(ev)); // no errors

        parser_free(&parser);
        errvec_free(&ev);
    }
}

TEST(parser_files, reject) {
    std::vector<neo::source_code> sources {load_all_source_files_from_dir("test/files/parser/reject")};
    for (auto &&src : sources) {
        std::cout << "Parsing invalid: " << src.get_file_name() << std::endl;
        error_vector_t ev;
        errvec_init(&ev);
        parser_t parser;
        parser_init(&parser, &ev);
        parser_setup_source(&parser, *src);

        astref_t root {parser_drain(&parser)};
        ASSERT_FALSE(astref_isnull(root));
        errvec_print(&ev, stdout, true);
        ASSERT_FALSE(errvec_isempty(ev)); // yes errors

        parser_free(&parser);
        errvec_free(&ev);
    }
}
