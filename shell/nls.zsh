# nls zsh integration
#
# When the current command line begins with "nls ", pressing Enter sends the
# rest of the line to the `nls` executable and replaces the buffer with the
# generated command, WITHOUT executing anything. You can then inspect/edit it
# and press Enter again to run it normally.
#
# Install:
#   source /path/to/nls/shell/nls.zsh
# (add that line to your ~/.zshrc to make it permanent)

_nls_accept_line() {
    emulate -L zsh

    # Only intercept lines that start with "nls " followed by a request.
    if [[ "$BUFFER" == "nls "* ]]; then
        local request="${BUFFER#nls }"

        # If there is no actual request, behave normally.
        if [[ -z "${request//[[:space:]]/}" ]]; then
            zle accept-line
            return
        fi

        local generated errfile ret
        errfile="$(mktemp -t nls_err.XXXXXX)" || { zle accept-line; return; }

        # `command nls` bypasses any function/alias named nls.
        # `-- "$request"` passes the raw string as a single argument, so shell
        # metacharacters in the request are never parsed by zsh.
        generated="$(command nls -- "$request" 2>"$errfile")"
        ret=$?

        if (( ret != 0 )) || [[ -z "$generated" ]]; then
            local msg
            msg="$(<"$errfile")"
            rm -f "$errfile"
            # Leave the user's original "nls ..." line intact and show why.
            zle -M "${msg:-nls: generation failed}"
            return
        fi
        rm -f "$errfile"

        BUFFER="$generated"
        CURSOR=${#BUFFER}
        zle redisplay
        return
    fi

    # Not an nls request: normal Enter behaviour.
    zle accept-line
}

zle -N _nls_accept_line
# Bind both Return (^M) and newline (^J): some terminals (e.g. Alacritty)
# send ^J for Enter rather than ^M.
bindkey '^M' _nls_accept_line
bindkey '^J' _nls_accept_line
