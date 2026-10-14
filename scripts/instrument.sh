#!/usr/bin/env bash
set -euo pipefail

# Compile one C++ source file, run BTAP, verify IR, optimize it, and link the
# reporting runtime. Set BTAP_CLANGXX and BTAP_OPT when matching tools are in
# different prefixes. BTAP_FINAL_OPT may be O0 or O2 (default: O2).
if [[ $# -ne 3 ]]; then
  echo "usage: $0 <source.cpp> <BTAPBoundsCheck.so> <output-executable>" >&2
  exit 64
fi

source_file=$1
plugin=$2
output=$3
llvm_bin=${BTAP_LLVM_BIN:-}
clang=${BTAP_CLANGXX:-"${llvm_bin:+$llvm_bin/}clang++"}
opt=${BTAP_OPT:-"${llvm_bin:+$llvm_bin/}opt"}
final_opt=${BTAP_FINAL_OPT:-O2}
case "$final_opt" in
  O0|O2) ;;
  *) echo "BTAP_FINAL_OPT must be O0 or O2" >&2; exit 64 ;;
esac
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

"$clang" -g -O0 -Xclang -disable-O0-optnone -S -emit-llvm "$source_file" -o "$work_dir/input.ll"
"$opt" -load-pass-plugin "$plugin" -passes=btap-bounds-check -verify-each -S \
  "$work_dir/input.ll" -o "$work_dir/instrumented.ll"
instrumented="$work_dir/instrumented.ll"
if [[ ${BTAP_RUN_TWICE:-0} == 1 ]]; then
  first_report_calls=$(rg -c 'call.*@btap_report_oob' "$instrumented" || true)
  first_report_calls=${first_report_calls:-0}
  "$opt" -load-pass-plugin "$plugin" -passes=btap-bounds-check -verify-each -S \
    "$instrumented" -o "$work_dir/instrumented-twice.ll"
  instrumented="$work_dir/instrumented-twice.ll"
  if [[ ${BTAP_ASSERT_IDEMPOTENT:-0} == 1 ]]; then
    second_report_calls=$(rg -c 'call.*@btap_report_oob' "$instrumented" || true)
    second_report_calls=${second_report_calls:-0}
    [[ "$first_report_calls" == "$second_report_calls" ]]
  fi
fi
if [[ -n ${BTAP_OUTPUT_IR:-} ]]; then
  cp "$instrumented" "$BTAP_OUTPUT_IR"
fi
"$opt" -passes="default<$final_opt>" -verify-each -S "$instrumented" -o "$work_dir/optimized.ll"
"$clang" "-$final_opt" "$work_dir/optimized.ll" "$root/runtime/btap_runtime.cpp" -o "$output"
