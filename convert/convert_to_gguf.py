#!/usr/bin/env python3
"""Convert local Llama weights into a GGUF for nls.

This wraps llama.cpp's `convert_hf_to_gguf.py` (run with PyTorch) to turn a
local HuggingFace-format Llama checkpoint directory into a GGUF file, then
optionally quantizes it with `llama-quantize`.

It downloads NOTHING from Hugging Face. The only network access is a shallow
clone of the llama.cpp GitHub repo (for the converter), pinned to the same
build as the installed runtime. You supply the model weights yourself.

Example:
    uv run python convert_to_gguf.py \\
        --src /path/to/Llama-3.2-3B-Instruct \\
        --out "$HOME/.local/share/nls/model.gguf" \\
        --outtype f16 \\
        --quantize Q4_K_M
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Pin the converters to the same llama.cpp build as the brew-installed runtime.
# Override with NLS_LLAMA_CPP_REF if you upgrade the runtime.
DEFAULT_REF = "b10470"
LLAMA_REPO = "https://github.com/ggml-org/llama.cpp"

# Relative paths (within the llama.cpp repo) of the two converters we support.
HF_CONVERTER = "convert_hf_to_gguf.py"
LEGACY_CONVERTER = "examples/convert_legacy_llama.py"

CACHE_DIR = Path(__file__).resolve().parent / ".cache"


def eprint(*args: object) -> None:
    print(*args, file=sys.stderr)


def resolve_repo(ref: str, refresh: bool) -> Path:
    """Return a local llama.cpp checkout at `ref`, shallow-cloning if needed.

    Modern converters (b10470+) import a sibling `conversion/` package and the
    bundled `gguf-py`, so we run them from a real checkout rather than fetching
    a single file. NLS_LLAMA_CPP_DIR overrides with an existing checkout.
    """
    override = os.environ.get("NLS_LLAMA_CPP_DIR")
    if override:
        p = Path(override).expanduser()
        if not (p / HF_CONVERTER).is_file():
            raise FileNotFoundError(
                f"NLS_LLAMA_CPP_DIR does not look like a llama.cpp checkout: {p}"
            )
        return p

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    repo = CACHE_DIR / f"llama.cpp-{ref}"
    if repo.is_dir() and (repo / HF_CONVERTER).is_file() and not refresh:
        return repo
    if repo.exists():
        shutil.rmtree(repo, ignore_errors=True)

    eprint(f"nls-convert: cloning llama.cpp @ {ref} (shallow) ...")
    subprocess.run(
        [
            "git",
            "clone",
            "--depth",
            "1",
            "--branch",
            ref,
            LLAMA_REPO,
            str(repo),
        ],
        check=True,
    )
    return repo


def resolve_convert_script(ref: str, relpath: str, refresh: bool) -> Path:
    """Return the path to a converter script inside a llama.cpp checkout."""
    override = os.environ.get("NLS_CONVERT_SCRIPT")
    if override:
        p = Path(override)
        if not p.is_file():
            raise FileNotFoundError(
                f"NLS_CONVERT_SCRIPT points at a missing file: {p}"
            )
        return p

    repo = resolve_repo(ref, refresh)
    script = repo / relpath
    if not script.is_file():
        raise FileNotFoundError(f"converter not found in checkout: {script}")
    return script


def detect_format(src: Path) -> str:
    """Return 'hf' or 'meta' based on the layout of the model directory."""
    if (src / "config.json").is_file():
        return "hf"
    if (src / "params.json").is_file() and any(src.glob("consolidated.*.pth")):
        return "meta"
    return "unknown"


def run(cmd: list[str]) -> None:
    eprint("nls-convert: $", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert local Llama weights into a GGUF for nls."
    )
    parser.add_argument(
        "--src",
        required=True,
        help="path to a local HF-format model directory (config.json + weights)",
    )
    parser.add_argument(
        "--out",
        required=True,
        help="output GGUF path",
    )
    parser.add_argument(
        "--outtype",
        default="f16",
        help="converter output type: f32, f16, bf16, q8_0 (default: f16)",
    )
    parser.add_argument(
        "--quantize",
        default=None,
        help="optional llama-quantize type, e.g. Q4_K_M or Q5_K_M",
    )
    parser.add_argument(
        "--llama-quantize",
        default=os.environ.get("NLS_LLAMA_QUANTIZE", "llama-quantize"),
        help="path to llama-quantize (default: found on PATH)",
    )
    parser.add_argument(
        "--ref",
        default=os.environ.get("NLS_LLAMA_CPP_REF", DEFAULT_REF),
        help=f"llama.cpp git ref for the converter (default: {DEFAULT_REF})",
    )
    parser.add_argument(
        "--vocab-type",
        default="bpe",
        help="vocab type for Meta original checkpoints (default: bpe, "
        "correct for Llama 3.x); ignored for HF-format models",
    )
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="re-download the converter script even if cached",
    )
    args = parser.parse_args()

    src = Path(args.src).expanduser()
    if not src.is_dir():
        eprint(f"nls-convert: --src is not a directory: {src}")
        return 2
    fmt = detect_format(src)
    if fmt == "unknown":
        eprint(
            f"nls-convert: {src} is neither an HF-format model (config.json) "
            "nor a Meta original checkpoint (params.json + consolidated.*.pth)."
        )
        return 2

    out = Path(args.out).expanduser()
    out.parent.mkdir(parents=True, exist_ok=True)

    relpath = HF_CONVERTER if fmt == "hf" else LEGACY_CONVERTER
    try:
        script = resolve_convert_script(args.ref, relpath, args.refresh)
    except Exception as exc:  # noqa: BLE001 - report and exit cleanly
        eprint(f"nls-convert: could not obtain converter script: {exc}")
        return 3

    # If we are quantizing, convert to an intermediate high-precision GGUF
    # first, then quantize that down to the requested type.
    convert_out = out
    if args.quantize:
        convert_out = out.with_name(out.stem + f".{args.outtype}.gguf")

    convert_cmd = [
        sys.executable,
        str(script),
        str(src),
        "--outfile",
        str(convert_out),
        "--outtype",
        args.outtype,
    ]
    if fmt == "meta":
        eprint(f"nls-convert: detected Meta original checkpoint in {src}")
        convert_cmd += ["--vocab-type", args.vocab_type]

    try:
        run(convert_cmd)
    except subprocess.CalledProcessError as exc:
        eprint(f"nls-convert: conversion failed (exit {exc.returncode})")
        return 4

    if args.quantize:
        quantize = shutil.which(args.llama_quantize) or args.llama_quantize
        if not shutil.which(quantize) and not Path(quantize).is_file():
            eprint(
                f"nls-convert: llama-quantize not found ({args.llama_quantize}); "
                "install llama.cpp or pass --llama-quantize."
            )
            return 5
        try:
            run([quantize, str(convert_out), str(out), args.quantize])
        except subprocess.CalledProcessError as exc:
            eprint(f"nls-convert: quantization failed (exit {exc.returncode})")
            return 6
        # Remove the large intermediate file.
        try:
            convert_out.unlink()
        except OSError:
            pass

    eprint(f"nls-convert: wrote {out}")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
