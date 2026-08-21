#!/usr/bin/env bash
# Manual integration test for nls.
#
# This exercises the real model and llama-cli, so it is NOT run by `make test`.
# It requires NLS_MODEL to point at a local .gguf instruction model and
# llama-cli to be installed (or NLS_LLAMA_CLI to be set).
#
# Usage:
#   NLS_MODEL=/path/to/model.gguf ./tests/integration.sh

set -u

NLS_BIN="${NLS_BIN:-./build/nls}"

if [[ ! -x "$NLS_BIN" ]]; then
    echo "nls binary not found at $NLS_BIN (run 'make' first)" >&2
    exit 1
fi

if [[ -z "${NLS_MODEL:-}" ]]; then
    echo "NLS_MODEL is not set; skipping model integration test." >&2
    echo "Set NLS_MODEL=/path/to/model.gguf to run it." >&2
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
    out="$("$NLS_BIN" -- "$req")"
    rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "  ERROR: nls exited with $rc" >&2
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
