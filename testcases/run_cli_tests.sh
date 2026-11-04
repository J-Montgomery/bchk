#!/usr/bin/env bash
set -euo pipefail

TOOL="${1:?Usage: $0 <path-to-tool>}"

for f in testcases/*; do
  [ -f "$f" ] || continue

  run_line=$(grep -m1 '^// *RUN:' "$f" || true)
  [ -z "$run_line" ] && continue
  
  cmd=$(echo "$run_line" | sed 's/.*RUN: *//')
  cmd="${cmd//%bchk/$TOOL}"
  cmd="${cmd//%s/$f}"

  expected=$(grep '^// *CHECK:' "$f" | sed 's/.*CHECK: *//' || true)
  actual=$(eval "$cmd" 2>&1 || true)

  if ! diff -u <(echo "$expected") <(echo "$actual") > /dev/null; then
    echo "FAIL: $(basename "$f")"
    echo "--- Diff ---"
    diff -u <(echo "$expected") <(echo "$actual") || true
    echo "------------"
    exit 1
  fi
done

echo "All tests passed!"