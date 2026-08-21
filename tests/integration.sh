#!/usr/bin/env bash
# Manual integration test for nsh.
#
# This exercises the real model via apfel, so it is NOT run by `make test`.
# It requires apfel to be installed and Apple Intelligence to be enabled.
#
# Usage:
#   ./tests/integration.sh

set -u

NSH_BIN="${NSH_BIN:-./build/nsh}"

if [[ ! -x "$NSH_BIN" ]]; then
    echo "nsh binary not found at $NSH_BIN (run 'make' first)" >&2
    exit 1
fi

if ! command -v apfel >/dev/null 2>&1 && [[ -z "${NSH_APFEL:-}" ]]; then
    echo "apfel not found; skipping model integration test." >&2
    echo "Install apfel (e.g. 'brew install apfel') to run it." >&2
    exit 0
fi

requests=(
    "find all files containing triggerOp("
    "find recursively for triggerOp( but ignore node_modules"
    "commits on this branch not main"
    "show the 20 largest files under here"
    "find all rust files modified in the last week containing unsafe"
)

fail=0
for req in "${requests[@]}"; do
    echo "----------------------------------------"
    echo "request: $req"
    out="$("$NSH_BIN" -- "$req")"
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "  ERROR: nsh exited with $rc" >&2
        fail=1
        continue
    fi
    if [[ -z "$out" ]]; then
        echo "  ERROR: empty output" >&2
        fail=1
        continue
    fi
    lines=$(printf '%s\n' "$out" | grep -c .)
    echo "  command: $out"
    if [[ "$lines" -ne 1 ]]; then
        echo "  WARNING: expected a single-line command, got $lines lines" >&2
    fi
done

echo "----------------------------------------"
if [[ $fail -eq 0 ]]; then
    echo "Integration run completed. Review the generated commands above."
else
    echo "Integration run had errors." >&2
fi
exit $fail
