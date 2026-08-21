# nsh zsh integration
#
# When the current command line begins with "nsh ", pressing Enter sends the
# rest of the line to the `nsh` executable and replaces the buffer with the
# generated command, WITHOUT executing anything. You can then inspect/edit it
# and press Enter again to run it normally.
#
# Install:
#   source /path/to/nsh/shell/nsh.zsh
# (add that line to your ~/.zshrc to make it permanent)

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

        # nsh lives in ~/.nsh; override with NSH_BIN (e.g. a dev build).
        local nsh_bin="${NSH_BIN:-$HOME/.nsh/nsh}"
        if [[ ! -x "$nsh_bin" ]]; then
            zle -M "nsh: binary not found at $nsh_bin (set NSH_BIN or reinstall)"
            return
        fi

        local generated errfile ret
        errfile="$(mktemp -t nsh_err.XXXXXX)" || { zle accept-line; return; }

        # `-- "$request"` passes the raw string as a single argument, so shell
        # metacharacters in the request are never parsed by zsh.
        generated="$("$nsh_bin" -- "$request" 2>"$errfile")"
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

        BUFFER="$generated"
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
