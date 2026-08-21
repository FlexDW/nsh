#!/bin/sh
# nsh installer.
#
# Downloads the nsh binary, system prompt, and zsh integration from the latest
# GitHub release into ~/.nsh, then wires up the zsh integration. nsh is NOT put
# on your PATH; the integration runs it from ~/.nsh only when a command line
# starts with "nsh ".
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/FlexDW/nsh/main/install.sh | sh
#
# Environment:
#   NSH_DIR   install location (default: ~/.nsh)

set -eu

REPO="FlexDW/nsh"
DEST="${NSH_DIR:-$HOME/.nsh}"
BASE="https://github.com/$REPO/releases/latest/download"

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) ;;
    *)
        echo "error: nsh requires macOS on Apple Silicon (arm64)." >&2
        exit 1
        ;;
esac

echo "Installing nsh into $DEST ..."
mkdir -p "$DEST"

fetch() {
    # $1 = remote asset name, $2 = local destination
    curl -fsSL -o "$2" "$BASE/$1" || {
        echo "error: failed to download $1 from $BASE" >&2
        exit 1
    }
}

fetch nsh-macos-arm64 "$DEST/nsh"
fetch system.txt "$DEST/system.txt"
fetch nsh.zsh "$DEST/nsh.zsh"
chmod +x "$DEST/nsh"

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
