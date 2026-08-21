# nsh - Natural Shell
#
# When the current command line begins with "nsh ", pressing Enter replaces it
# with a shell command generated from the rest of the line by Apple Intelligence
# (via apfel), WITHOUT executing anything. Inspect/edit it, then press Enter
# again to run it yourself.
#
# Install:
#   source ~/.nsh/nsh.zsh
# (add that line to your ~/.zshrc to make it permanent)

# Generate a command from a natural-language request; prints it on stdout.
_nsh_generate() {
    emulate -L zsh
    local request="$1"
    local prompt_file="$HOME/.nsh/system.txt"

    if [[ ! -r "$prompt_file" ]]; then
        print -r -- "nsh: system prompt not found at $prompt_file" >&2
        return 1
    fi
    if ! command -v apfel >/dev/null 2>&1; then
        print -r -- "nsh: apfel not found (brew install apfel)" >&2
        return 1
    fi

    local system
    system="$(<"$prompt_file")"
    system+=$'\n\nContext:\n'"cwd: $PWD"$'\nshell: zsh\nos: macOS\n'"architecture: $(uname -m)"

    # --code prints just the command; -- passes the request as one argument so
    # its shell metacharacters are never interpreted.
    apfel -q --temperature 0 --max-tokens 150 --code -s "$system" -- "$request"
}

_nsh_accept_line() {
    emulate -L zsh

    # Only intercept lines that start with "nsh " followed by a request.
    if [[ "$BUFFER" == "nsh "* ]]; then
        local request="${BUFFER#nsh }"

        # If there is no actual request, behave normally.
        if [[ -z "${request//[[:space:]]/}" ]]; then
            zle accept-line
            return
        fi

        local generated errfile ret
        errfile="$(mktemp -t nsh_err.XXXXXX)" || { zle accept-line; return; }

        generated="$(_nsh_generate "$request" 2>"$errfile")"
        ret=$?

        if (( ret != 0 )) || [[ -z "$generated" ]]; then
            local msg
            msg="$(<"$errfile")"
            rm -f "$errfile"
            # Leave the user's original "nsh ..." line intact and show why.
            zle -M "${msg:-nsh: generation failed}"
            return
        fi
        rm -f "$errfile"

        # Keep the first line only.
        BUFFER="${generated%%$'\n'*}"
        CURSOR=${#BUFFER}
        zle redisplay
        return
    fi

    # Not an nsh request: normal Enter behaviour.
    zle accept-line
}

zle -N _nsh_accept_line
# Bind both Return (^M) and newline (^J): some terminals (e.g. Alacritty)
# send ^J for Enter rather than ^M.
bindkey '^M' _nsh_accept_line
bindkey '^J' _nsh_accept_line
