# Natural Shell

`nsh` translates a natural-language request into a shell command using
**Apple Intelligence**, invoked on-device through the `apfel` CLI — no HTTP
API, no Ollama, no GGUF files, no cloud calls. Inference runs locally on your
Mac.

```
$ nsh find all files containing triggerOp( but ignore node_modules
```

With the zsh integration enabled, pressing Enter replaces your command line
with a generated command that you can inspect and edit:

```
$ rg -l -F 'triggerOp(' . -g '!node_modules/**'
```

You then press Enter a second time to run it yourself.

## Quick start

Install `apfel` and enable Apple Intelligence, then run the installer:

```sh
# apfel is the Apple Intelligence CLI that nsh calls at runtime
brew install apfel
# then: System Settings > Apple Intelligence & Siri > turn ON and let the
# on-device model download.

# Install nsh (binary + prompt + zsh integration) into ~/.nsh and wire up zsh
curl -fsSL https://raw.githubusercontent.com/FlexDW/nsh/main/install.sh | sh
```

Start a new shell (or `source ~/.nsh/nsh.zsh`), then type a request and press
Enter — the integration replaces your line with the generated command for you
to review:

```
$ nsh find all rust files modified in the last week containing unsafe
```

`nsh` is **not** put on your `PATH`. The zsh integration runs it from `~/.nsh`
only for lines that begin with `nsh `. Edit `~/.nsh/system.txt` to tune the
prompt.

## Install from a release

The installer pulls from the latest
[GitHub release](https://github.com/FlexDW/nsh/releases), which carries
`nsh-macos-arm64`, `system.txt`, `nsh.zsh`, and `install.sh`. To install by
hand:

```sh
mkdir -p ~/.nsh && cd ~/.nsh
base=https://github.com/FlexDW/nsh/releases/latest/download
curl -fsSL -o nsh "$base/nsh-macos-arm64" && chmod +x nsh
curl -fsSL -o system.txt "$base/system.txt"
curl -fsSL -o nsh.zsh "$base/nsh.zsh"
echo 'source "$HOME/.nsh/nsh.zsh"' >> ~/.zshrc
```

## Build from source

```sh
xcode-select --install     # C++ toolchain, once
make                       # -> build/nsh
make test                  # unit tests
```

Point the integration at your local build with `NSH_BIN`:

```sh
NSH_BIN="$PWD/build/nsh" source shell/nsh.zsh
```

Cutting a release: build the binary locally with `make dist` (writes
`dist/nsh-macos-arm64`) and commit it, then run the **Release** workflow
([.github/workflows/release.yml](.github/workflows/release.yml)) from the
Actions tab, choosing a `patch`/`minor`/`major` bump (or a `version_override`
for the first release). It runs on Linux and publishes the committed binary
plus `system.txt`, `nsh.zsh`, and `install.sh` — no macOS CI minutes needed.

## 1. What it does

`nsh` takes a natural-language description of what you want to do and prints a
single shell command that does it. It is designed for zsh on macOS (Apple
Silicon), and runs entirely on-device through Apple Intelligence.

## 2. Safety model

**`nsh` generates text. It never executes the generated command.**

- The generated command is only written to stdout (or inserted into your zsh
  command buffer for you to review).
- `nsh` never calls `eval`, `exec`, `system()`, or `popen()` on generated text.
- The only subprocess `nsh` launches is `apfel`, invoked via `fork`/`execv`
  with an explicit argument vector — your request is passed as a single
  argument and is never interpreted by a shell.
- The zsh integration replaces the command buffer and stops; it does **not**
  auto-execute. Nothing runs until you press Enter yourself.

## 3. Dependencies

- macOS with Apple Intelligence supported and enabled (Apple Silicon).
- A C++17 compiler (Apple Clang from the Xcode Command Line Tools:
  `xcode-select --install`).
- The `apfel` CLI at runtime (Apple Intelligence from the command line):

  ```sh
  brew install apfel
  ```

  If it is not on your `PATH`, set `NSH_APFEL=/path/to/apfel`.
- Apple Intelligence turned on (System Settings > Apple Intelligence & Siri),
  with the on-device model downloaded. `nsh` sends nothing to the cloud.

## 4. Building

```sh
make        # builds build/nsh
make test   # builds and runs the unit tests
```

To use your local build with the zsh integration, point `NSH_BIN` at it:

```sh
NSH_BIN="$PWD/build/nsh" source shell/nsh.zsh
```

`make dist` stages the binary at `dist/nsh-macos-arm64` for a release (see
[Install from a release](#install-from-a-release)).

## 5. Model backend

`nsh` uses Apple Intelligence's on-device model through `apfel`; there is no
model file to supply. `apfel` handles the chat template and returns just the
generated command, so `nsh` only appends runtime context (`cwd`, `os`,
`architecture`) to the system prompt.

Tunables (all optional, see [config/example.conf](config/example.conf)):

- `NSH_APFEL` — path to apfel (default: found on `PATH`).
- `NSH_MAX_TOKENS` — max output tokens (default: 150).
- `NSH_TEMPERATURE` — sampling temperature; 0 = deterministic (default: 0).
- `NSH_APFEL_ARGS` — extra args passed through to apfel.

If Apple Intelligence is not enabled, `apfel` fails and `nsh` reports the
error; run with `NSH_DEBUG=1` to see apfel's diagnostics.

## 6. Installing the zsh integration

The installer adds the source line for you. To do it by hand, add one of these
to your `~/.zshrc`:

```sh
# installed from a release (install.sh) — binary lives in ~/.nsh
source "$HOME/.nsh/nsh.zsh"

# or straight from the repo, pointing at your local build
NSH_BIN="/path/to/nsh/build/nsh" source /path/to/nsh/shell/nsh.zsh
```

Reload your shell (`exec zsh`) and you're set. The integration only affects
lines that begin with `nsh `; every other command behaves exactly as before.

## 7. Example usage

Direct CLI (prints the command, does not run it):

```sh
$ nsh "show commits on this branch that aren't on main"
git log main..HEAD --oneline
```

Requests may be quoted or passed as separate words; use `--` to pass a request
that starts with `-` or contains shell metacharacters verbatim:

```sh
$ nsh -- "foo && bar"
```

With the zsh integration, just type and press Enter:

```
$ nsh find the 20 largest files under here
# becomes, after Enter:
$ find . -type f -exec du -h {} + | sort -rh | head -n 20
```

More requests to try:

```
nsh find all files containing triggerOp(
nsh find recursively for triggerOp( but ignore node_modules
nsh commits on this branch not main
nsh find all rust files modified in the last week containing unsafe
```

## 8. Architecture

```
nsh (C++17 executable)
├── args.{h,cpp}   argument parsing (quoted / multi-word / -- passthrough)
├── clean.{h,cpp}  output cleanup (trim, strip code fence, first line, validate)
└── nsh.cpp        prompt building + inference subprocess + main
```

Flow:

1. Parse the request (all argv joined; everything after `--` is verbatim).
2. Load the system prompt from `~/.nsh/system.txt` (erroring if it is missing)
   and append dynamic context: `cwd`, `shell`, `os`, `architecture`. No
   directory listings, file contents, secrets, or arbitrary environment
   variables are sent to the model.
3. Run inference by invoking `apfel` as a subprocess (system prompt via `-s`,
   request as the positional argument after `--`), capturing stdout. This is
   isolated in `run_inference()` so it can be replaced later.
4. Strip a single surrounding Markdown fence and a stray `$ ` prompt, take the
   first non-empty line, and reject empty output.
5. Print the command to stdout; all diagnostics go to stderr.

**Inference defaults:** deterministic (`--temperature 0`), ~150 output tokens,
single turn, no conversation history. Tunable via `NSH_MAX_TOKENS`,
`NSH_TEMPERATURE`, and `NSH_APFEL_ARGS`.

### Why a subprocess

`nsh` shells out to `apfel` rather than linking any inference library directly.
This keeps the build to a single-file Makefile with no external build
dependencies. The subprocess boundary lives entirely in `run_inference()`, so a
different backend (another local model, a cloud API) can replace it without
touching argument parsing, prompt building, or output cleanup.

## 9. Known limitations

- Requires `apfel` on `PATH` (or `NSH_APFEL`) and Apple Intelligence enabled.
- Output quality depends on Apple's on-device model. Always review before
  running.
- zsh + macOS only. bash/fish/Linux are out of scope.
- `NSH_APFEL_ARGS` is split on whitespace only (no quoting).

## Testing

- `make test` runs unit tests for argument parsing, output cleanup, Markdown
  fence stripping, and empty-output handling. These do not require a model.
- [tests/integration.sh](tests/integration.sh) is a manual, model-dependent
  script (not run by `make test`):

  ```sh
  ./tests/integration.sh
  ```
