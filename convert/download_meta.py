#!/usr/bin/env python3
"""Download Llama original checkpoints directly from Meta's CDN (no Hugging Face).

You accept Meta's license at https://www.llama.com/llama-downloads and Meta
gives you a per-user *signed URL* containing a literal '*' placeholder, e.g.

    https://llama3-2-lightweight.llamameta.net/*?Policy=...&Signature=...&Key-Pair-Id=...

This tool downloads a model's original-format files by substituting that '*'
with '<model-path>/<file>' for each file, exactly like Meta's own downloader.

The signed URL is sensitive (it grants download access). Pass it via the
LLAMA_SIGNED_URL environment variable or the interactive prompt. Do NOT paste
it where it could be logged.

Example:
    LLAMA_SIGNED_URL='https://.../*?Policy=...' \\
    uv run python download_meta.py \\
        --model-path Llama3.2-1B-Instruct \\
        --dest ./weights/Llama3.2-1B-Instruct
"""

from __future__ import annotations

import argparse
import getpass
import os
import sys
import urllib.request
from pathlib import Path

# Original-format files that make up a single-shard Llama 3.x model.
DEFAULT_FILES = [
    "params.json",
    "tokenizer.model",
    "consolidated.00.pth",
    "checklist.chk",
]


def eprint(*args: object) -> None:
    print(*args, file=sys.stderr)


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    eprint(f"nls-download: {dest.name} ...")
    with urllib.request.urlopen(url) as resp:
        length = resp.headers.get("Content-Length")
        expected = int(length) if length is not None else None
        written = 0
        with open(tmp, "wb") as out:
            while True:
                chunk = resp.read(1 << 20)
                if not chunk:
                    break
                out.write(chunk)
                written += len(chunk)
    # Guard against silently truncated downloads (urllib does not verify this).
    if expected is not None and written != expected:
        tmp.unlink(missing_ok=True)
        raise IOError(f"truncated download: got {written} of {expected} bytes")
    tmp.replace(dest)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download Llama original weights from Meta's CDN."
    )
    parser.add_argument(
        "--model-path",
        required=True,
        help="Meta model directory name, e.g. Llama3.2-1B-Instruct",
    )
    parser.add_argument(
        "--dest",
        required=True,
        help="local directory to write the checkpoint files into",
    )
    parser.add_argument(
        "--files",
        nargs="*",
        default=DEFAULT_FILES,
        help="files to download (default: single-shard Llama 3.x set)",
    )
    parser.add_argument(
        "--url",
        default=os.environ.get("LLAMA_SIGNED_URL"),
        help="signed URL (default: $LLAMA_SIGNED_URL, else prompted)",
    )
    args = parser.parse_args()

    url = args.url
    if not url:
        # Read without echoing to keep the signed URL out of shell history/logs.
        url = getpass.getpass("Paste the Meta signed URL (input hidden): ").strip()
    if "*" not in url:
        eprint(
            "nls-download: the signed URL must contain a '*' placeholder "
            "(the one Meta gives you). Got a URL without '*'."
        )
        return 2

    dest = Path(args.dest).expanduser()
    for f in args.files:
        file_url = url.replace("*", f"{args.model_path}/{f}", 1)
        try:
            download(file_url, dest / f)
        except Exception as exc:  # noqa: BLE001 - report and continue-fail
            eprint(f"nls-download: failed on {f}: {exc}")
            if f == "checklist.chk":
                eprint("nls-download: (checklist.chk is optional; continuing)")
                continue
            return 3

    eprint(f"nls-download: wrote checkpoint to {dest}")
    print(dest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
