# Natural Language Shell

`nls` translates a natural-language request into a shell command using a
**local** GGUF instruction model. It runs fully offline via
[`llama.cpp`](https://github.com/ggml-org/llama.cpp) — no HTTP API, no Ollama,
no cloud calls, no Hugging Face dependency at runtime.

```
$ nls find all files containing triggerOp( but ignore node_modules
```

With the zsh integration enabled, pressing Enter replaces your command line
with a generated command that you can inspect and edit:

```
$ rg -l -F 'triggerOp(' . -g '!node_modules/**'
```

You then press Enter a second time to run it yourself.

## Quick start

```sh
# 1. Toolchain (once): Xcode Command Line Tools for the C++ compiler
xcode-select --install

# 2. Install the local inference runtime (provides llama-simple)
brew install llama.cpp

# 3. Build and install nls
make install                       # -> ~/.local/bin/nls  (+ ~/.local/share/nls)

# 4. Point nls at a local GGUF instruction model (you supply this file)
export NLS_MODEL="$HOME/.local/share/nls/model.gguf"

# 5. Enable the zsh integration (add to ~/.zshrc to persist)
source "$HOME/.local/share/nls/nls.zsh"
```

Then type a request and press Enter:

```
$ nls find all rust files modified in the last week containing unsafe
```

`llama.cpp` is a community/`ggml-org` project (not Meta), and Homebrew is a
third-party package manager — both are just redistributing the open-source
runtime. See [Dependencies](#3-dependencies) for the manual/download
alternatives. `nls` never downloads a model; obtaining a GGUF is up to you.

## 1. What it does

`nls` takes a natural-language description of what you want to do and prints a
single shell command that does it. It is designed for zsh on macOS (Apple
Silicon supported through llama.cpp/Metal).

## 2. Safety model

**`nls` generates text. It never executes the generated command.**

- The generated command is only written to stdout (or inserted into your zsh
  command buffer for you to review).
- `nls` never calls `eval`, `exec`, `system()`, or `popen()` on generated text.
- The only subprocess `nls` launches is the local inference runtime
  (`llama-simple`), invoked via `fork`/`execv` with an explicit argument vector —
  your request is passed as a single argument and is never interpreted by a
  shell.
- The zsh integration replaces the command buffer and stops; it does **not**
  auto-execute. Nothing runs until you press Enter yourself.

## 3. Dependencies

- macOS (primary target; Apple Silicon supported).
- A C++17 compiler (Apple Clang from the Xcode Command Line Tools:
  `xcode-select --install`).
- `llama.cpp`'s `llama-simple` executable at runtime (it ships with llama.cpp).
  It is an open-source project by `ggml-org` (not Meta). Install it either way:

  ```sh
  brew install llama.cpp          # simplest on macOS (Homebrew, third-party)
  ```

  Or download an official prebuilt binary from the llama.cpp repo:
  <https://github.com/ggml-org/llama.cpp/releases> — grab the macOS Apple
  Silicon asset (`llama-b<NNNN>-bin-macos-arm64.zip`), unzip it, and either add
  its `bin/` to your `PATH` or set `NLS_LLAMA_SIMPLE=/path/to/llama-simple`.

  nls uses `llama-simple` (not the interactive `llama-cli`) so it gets clean,
  scriptable, single-shot output. If it is not on your `PATH`, set
  `NLS_LLAMA_SIMPLE=/path/to/llama-simple`.
- A local GGUF instruction model (see below). Obtaining a model is currently
  your responsibility; `nls` does not download anything.

## 4. Building

```sh
make        # builds build/nls
make test   # builds and runs the unit tests
```

Install into `~/.local`:

```sh
make install
# installs:
#   ~/.local/bin/nls
#   ~/.local/share/nls/system.txt
#   ~/.local/share/nls/nls.zsh
```

Make sure `~/.local/bin` is on your `PATH`. Override the location with
`make install PREFIX=/somewhere`.

## 5. Specifying a local GGUF model

`nls` needs a local GGUF instruction model. Point `NLS_MODEL` at it:

```sh
export NLS_MODEL="$HOME/.local/share/nls/model.gguf"   # any path you like
```

`nls` is model-agnostic. Any small-ish GGUF instruction model works, for
example Llama 3.2 1B/3B Instruct, or a Gemma-class small instruction model.
1–4B instruction models are a good latency/quality tradeoff.

If `NLS_MODEL` is unset, `nls` prints:

```
nls: NLS_MODEL is not set
nls: set NLS_MODEL=/path/to/model.gguf
```

See [config/example.conf](config/example.conf) for all supported environment
variables.

### Generating a GGUF from Llama weights (no Hugging Face)

If you don't already have a `.gguf`, the [convert/](convert/) subfolder turns
Llama weights into one using PyTorch and llama.cpp's converter. It is a
[`uv`](https://docs.astral.sh/uv/)-managed Python project (latest Python), and
it downloads nothing from Hugging Face — the converter comes from a shallow
clone of the llama.cpp GitHub repo, pinned to the same build as your runtime
(`b10470`).

One-time setup (installs PyTorch + the pinned converter into `convert/.venv`):

```sh
make convert-setup
```

**Option A — Meta's CDN (no Hugging Face).** Accept the license at
<https://www.llama.com/llama-downloads>, which gives you a per-user *signed
URL*. Then:

```sh
make model-meta META_MODEL=Llama-3.2-1B-Instruct MODEL_SIZE=1B
# or the sharper 3B:
make model-meta META_MODEL=Llama-3.2-3B-Instruct MODEL_SIZE=3B
```

You'll be prompted to paste the signed URL (input hidden), or you can
`export LLAMA_SIGNED_URL='https://.../*?Policy=...'` first. This downloads the
original checkpoint (`consolidated.00.pth`, `params.json`, `tokenizer.model`)
straight from Meta's CDN, then does an **original → HF → GGUF** conversion,
fully offline, and quantizes to a GGUF at `MODEL_OUT`. (Llama 3.x uses a
tiktoken tokenizer that requires the HF intermediate step; it is built locally
from your `tokenizer.model` with no Hugging Face access.)

**Option B — a local HF-format directory you already have** (`config.json` +
weights):

```sh
make model SRC_MODEL=/path/to/Llama-3.2-3B-Instruct-hf
```

Both write to `MODEL_OUT` (default `~/.local/share/nls/model.gguf`) and print
the `export NLS_MODEL=...` line to use. Tunables: `MODEL_OUT`, `OUTTYPE`
(default `f16`), `QUANTIZE` (default `Q4_K_M`), `MODEL_SIZE` (`1B`/`3B`). The
signed URL is sensitive — it is only ever read via the hidden prompt or the
environment, never logged.

## 6. Installing the zsh integration

Source the integration script from your `~/.zshrc`:

```sh
# if you ran `make install`
source "$HOME/.local/share/nls/nls.zsh"

# or straight from the repo
source /path/to/nls/shell/nls.zsh
```

Reload your shell (`exec zsh`) and you're set. The integration only affects
lines that begin with `nls `; every other command behaves exactly as before.

## 7. Example usage

Direct CLI (prints the command, does not run it):

```sh
$ nls "show commits on this branch that aren't on main"
git log main..HEAD --oneline
```

Requests may be quoted or passed as separate words; use `--` to pass a request
that starts with `-` or contains shell metacharacters verbatim:

```sh
$ nls -- "foo && bar"
```

With the zsh integration, just type and press Enter:

```
$ nls find the 20 largest files under here
# becomes, after Enter:
$ find . -type f -exec du -h {} + | sort -rh | head -n 20
```

More requests to try:

```
nls find all files containing triggerOp(
nls find recursively for triggerOp( but ignore node_modules
nls commits on this branch not main
nls find all rust files modified in the last week containing unsafe
```

`nls --help` and `nls --version` are also available.

## 8. Architecture

```
nls (C++17 executable)
├── args.{h,cpp}   argument parsing (quoted / multi-word / -- passthrough)
├── clean.{h,cpp}  output cleanup (trim, strip code fence, first line, validate)
└── nls.cpp        prompt building + inference subprocess + main
```

Flow:

1. Parse the request (all argv joined; everything after `--` is verbatim).
2. Load the system prompt (`prompts/system.txt`, an installed copy, or a
   built-in fallback) and append dynamic context: `cwd`, `shell`, `os`,
   `architecture`. No directory listings, file contents, secrets, or arbitrary
   environment variables are sent to the model.
3. Wrap it in the Llama 3 chat template and run inference by invoking
   `llama-simple` as a subprocess, capturing stdout. This is isolated in
   `run_inference()` so it can be replaced later.
4. Split the reply off the echoed prompt, drop Llama special tokens, strip a
   single surrounding Markdown fence and a stray `$ ` prompt, take the first
   non-empty line, and reject empty output.
5. Print the command to stdout; all diagnostics go to stderr.

**Inference defaults:** greedy/deterministic (llama-simple), ~150 output
tokens, single turn, no conversation history. Tunable via `NLS_MAX_TOKENS`,
`NLS_NGL`, and `NLS_LLAMA_ARGS`.

### Why a subprocess

V1 shells out to `llama-simple` rather than linking `libllama` directly. This
keeps the build to a single-file Makefile with no external build dependencies
and works with a stock `brew install llama.cpp`. (`llama-simple` is used rather
than the interactive `llama-cli`, which on recent builds forces a REPL and
pollutes stdout.) The subprocess boundary lives entirely in `run_inference()`,
so a direct `libllama` integration can replace it in V2 without touching
argument parsing, prompt building, or output cleanup.

## 9. Known limitations

- Requires `llama-simple` on `PATH` (or `NLS_LLAMA_SIMPLE`). No direct
  `libllama` linking yet.
- The model is loaded on every invocation, so there is per-call startup
  latency. A persistent model process is planned for V2.
- Output quality depends entirely on the chosen GGUF model; small models (e.g.
  Llama 3.2 1B) can produce imperfect commands. 3B is noticeably better. Always
  review before running.
- The prompt uses the Llama 3 chat template. Other model families may need a
  different template.
- zsh + macOS only. bash/fish/Linux are out of scope for V1.
- `NLS_LLAMA_ARGS` is split on whitespace only (no quoting).

## Testing

- `make test` runs unit tests for argument parsing, output cleanup, Markdown
  fence stripping, and empty-output handling. These do not require a model.
- [tests/integration.sh](tests/integration.sh) is a manual, model-dependent
  script (not run by `make test`):

  ```sh
  NLS_MODEL=/path/to/model.gguf ./tests/integration.sh
  ```
