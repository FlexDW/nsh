#!/bin/sh
# nsh installer.
#
# Downloads the zsh integration and system prompt into ~/.nsh and wires up zsh.
# nsh is not a binary; the integration runs Apple Intelligence via apfel, only
# when a command line starts with "nsh ".
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/FlexDW/nsh/main/install.sh | sh

set -eu

DEST="$HOME/.nsh"
BASE="https://raw.githubusercontent.com/FlexDW/nsh/main"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "error: nsh requires macOS." >&2
    exit 1
fi

echo "Installing nsh into $DEST ..."
mkdir -p "$DEST"

fetch() {
    # $1 = file name in the repo, $2 = local destination
    curl -fsSL -o "$2" "$BASE/$1" || {
        echo "error: failed to download $1 from $BASE" >&2
        exit 1
    }
}

fetch nsh.zsh "$DEST/nsh.zsh"
fetch system.txt "$DEST/system.txt"

# Wire up ~/.zshrc (idempotent).
ZSHRC="$HOME/.zshrc"
SOURCE_LINE="source \"$DEST/nsh.zsh\""
if [ -f "$ZSHRC" ] && grep -qF "$SOURCE_LINE" "$ZSHRC"; then
    echo "Integration already present in $ZSHRC"
else
    printf '\n# nsh (Natural Shell) integration\n%s\n' "$SOURCE_LINE" >> "$ZSHRC"
    echo "Added integration to $ZSHRC"
fi

if ! command -v apfel >/dev/null 2>&1; then
    echo
    echo "Note: apfel was not found. nsh needs it at runtime:"
    echo "  brew install apfel"
    echo "and Apple Intelligence must be enabled in System Settings."
fi

echo
echo "Done. Start a new shell (or run: source \"$DEST/nsh.zsh\"),"
echo "then type:  nsh <what you want to do>  and press Enter."
