#pragma once

#include <cstdint>

// C ABI called by the LLVM pass.  It always terminates the process with exit
// status 86, so execution can never continue with an invalid memory access.
extern "C" [[noreturn]] void btap_report_oob(std::int64_t index,
                                             std::uint64_t bound,
                                             const char *function_name,
                                             const char *source_file,
                                             std::uint32_t source_line,
                                             std::uint32_t source_column);
