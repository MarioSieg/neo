/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "../compiler/neo_compiler.h"

#include <string>

// Recommended cmd flags for my workstation:
// corpus/ -jobs=60 -workers=60 -max_len=16384 -only_ascii=1 -rss_limit_mb=16384 -max_total_time=10 -dict="fuzz/dict.txt" -exact_artifact_path="bin/fuzz"
// For running an hour: -max_total_time=3600

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) -> int {
    std::string src {data, data+size}; // convert to std::string, we require zero-termination

    Compiler *com;
    compiler_init(&com, static_cast<ComFlags>(COM_FLAG_SILENT | COM_FLAG_NO_AUTODUMP));
    bool result = 0 == compiler_compile_string(com, src.c_str(), "test.neo");
    compiler_free(&com);

    return result ? 0 : -1;
}
