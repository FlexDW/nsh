# Natural Shell (nsh)

`nsh` turns a natural-language request into a shell command using **Apple
Intelligence**, on-device via the [`apfel`](https://apfel.franzai.com) CLI — no
binary to build, no cloud calls. It's a small zsh integration: type a request,
press Enter, and your command line is replaced with the generated command for
you to review and run.

```
$ nsh kill the process on port 3000
# becomes, after Enter:
$ kill $(lsof -t -i :3000)
```

Nothing runs until you press Enter a second time yourself.

## Install

```sh
# 1. apfel — the Apple Intelligence CLI nsh calls
brew install apfel
#    then turn it on: System Settings > Apple Intelligence & Siri, and let the
#    on-device model download.

# 2. Configure nsh
curl -fsSL https://raw.githubusercontent.com/FlexDW/nsh/main/install.sh | sh
```
> What does this script do? It downloads `nsh.zsh` and `system.txt` into
> `~/.nsh` and appends `source "$HOME/.nsh/nsh.zsh"` to your `~/.zshrc`. No
> binary is installed and your `PATH` is left untouched.

Start a new shell (or `source ~/.nsh/nsh.zsh`) and you're set.

Manual install:

```sh
git clone --depth 1 https://github.com/FlexDW/nsh ~/.nsh
echo 'source "$HOME/.nsh/nsh.zsh"' >> ~/.zshrc
```

## How it works

nsh rebinds the **Enter key** to a zsh line-editor (ZLE) widget (bound to both
Return `^M` and newline `^J`). The widget only acts on command lines that start
with `nsh ` — every other line runs exactly as before. When you press Enter on
an `nsh …` line it:

1. reads the system prompt from `~/.nsh/system.txt` and appends context
   (`cwd`, `shell`, `os`, `architecture`);
2. runs `apfel --code -s <prompt> -- "<request>"` to generate one command;
3. replaces your command line with the result — it does **not** run it.

Press Enter again to run the generated command yourself. The request is passed
to `apfel` as a single `--`-terminated argument, so shell metacharacters in it
are never interpreted. Nothing is sent to the cloud; inference is on-device.

## Customize

- Edit `~/.nsh/system.txt` to change how commands are generated (which utilities
  to prefer, style, examples). See [system.txt](system.txt) for the default.
- The `apfel` flags (`--temperature`, `--max-tokens`) live in [nsh.zsh](nsh.zsh).

## Requirements

- macOS with Apple Intelligence enabled (Apple Silicon).
- `apfel` on your `PATH` (`brew install apfel`).
- zsh.

## Uninstall

```sh
rm -rf ~/.nsh
# then remove the `source "$HOME/.nsh/nsh.zsh"` line from ~/.zshrc
```
