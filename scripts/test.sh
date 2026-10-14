#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <BTAPBoundsCheck.so>" >&2
  exit 64
fi

plugin=$1
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
out_dir="$root/out"
mkdir -p "$out_dir"

for final_opt in O0 O2; do
  test_prefix="$out_dir/${final_opt,,}"
  mkdir -p "$test_prefix"

  BTAP_RUN_TWICE=1 BTAP_ASSERT_IDEMPOTENT=1 BTAP_FINAL_OPT="$final_opt" \
    "$root/scripts/instrument.sh" "$root/tests/valid.cpp" "$plugin" "$test_prefix/valid"
  [[ $("$test_prefix/valid") == 42 ]]

  BTAP_RUN_TWICE=1 BTAP_ASSERT_IDEMPOTENT=1 BTAP_FINAL_OPT="$final_opt" \
    "$root/scripts/instrument.sh" "$root/tests/one_past_unused.cpp" "$plugin" "$test_prefix/one_past_unused"
  "$test_prefix/one_past_unused"

  for test_case in invalid_write invalid_read; do
    BTAP_RUN_TWICE=1 BTAP_ASSERT_IDEMPOTENT=1 BTAP_FINAL_OPT="$final_opt" \
      "$root/scripts/instrument.sh" "$root/tests/$test_case.cpp" "$plugin" "$test_prefix/$test_case"
    for bad_index in -1 8; do
      set +e
      "$test_prefix/$test_case" "$bad_index" >"$test_prefix/$test_case.$bad_index.out" 2>&1
      status=$?
      set -e
      [[ $status -eq 86 ]]
      rg -q 'BTAP bounds-check failure' "$test_prefix/$test_case.$bad_index.out"
    done
  done

  unsupported_ir="$test_prefix/unsupported.ll"
  BTAP_RUN_TWICE=1 BTAP_ASSERT_IDEMPOTENT=1 BTAP_FINAL_OPT="$final_opt" \
    BTAP_OUTPUT_IR="$unsupported_ir" "$root/scripts/instrument.sh" \
    "$root/tests/unsupported.cpp" "$plugin" "$test_prefix/unsupported"
  ! rg -q '@btap_report_oob' "$unsupported_ir"
  "$test_prefix/unsupported" 7
done

echo "BTAP checks passed at final -O0 and -O2. Unsupported heap access remained uninstrumented."
