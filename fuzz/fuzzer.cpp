/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <neo_compiler.h>

#include <cstring>
#include <string>

// Recommended cmd flags:
// corpus/ -jobs=12 -workers=12 -max_len=16384 -detect_leaks=0 -rss_limit_mb=16384 -max_total_time=10 -dict="fuzz/dict.txt" -exact_artifact_path="bin/fuzz"
// For running with ASCII input to test the parser more instead of lexer (rejects invalid UTF-8): -only_ascii=1
// For running 10 minutes: -max_total_time=600
// For running an hour: -max_total_time=3600

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) -> int {
    bool is_zero = std::all_of(data, data + size,[](std::uint8_t byte) noexcept -> bool { return byte == 0; } );
    if (is_zero) { /* Skip empty inputs. */
        return -1;
    }

    std::u8string src {data, data+size}; // convert to std::string, we require zero-termination
    source_load_error_info_t info {};
    const source_t *source = source_from_memory_ref(
        reinterpret_cast<const std::uint8_t *>(u8"<fuzz.neo>"),
        reinterpret_cast<const std::uint8_t *>(src.c_str()),
        &info
    );
    if (info.unicode_error != NEO_UNIERR_OK) {
        return -1;
    }
    neo_compiler_t *com;
    compiler_init(&com, static_cast<neo_compiler_flag_t>(COM_FLAG_NO_STATUS | COM_FLAG_NO_ERROR_DUMP));
    bool result = compiler_compile(com, source, nullptr);
    compiler_free(&com);
    return result ? 0 : -1;
}
