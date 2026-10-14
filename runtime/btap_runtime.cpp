#include "btap_runtime.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

extern "C" [[noreturn]] void btap_report_oob(std::int64_t index,
                                               std::uint64_t bound,
                                               const char *function_name,
                                               const char *source_file,
                                               std::uint32_t source_line,
                                               std::uint32_t source_column) {
  std::fprintf(stderr,
               "BTAP bounds-check failure: index=%" PRId64
               ", bound=%" PRIu64 " in %s at %s:%" PRIu32 ":%" PRIu32 "\n",
               index, bound, function_name ? function_name : "<unknown>",
               source_file ? source_file : "<unknown>", source_line,
               source_column);
  std::fflush(stderr);
  std::exit(86);
}
