#!/usr/bin/env bash
# Convert Meta *original* Llama weights -> HuggingFace format, fully offline.
#
# Llama 3.x ships a tiktoken tokenizer that llama.cpp's legacy converter can't
# read, so the reliable path is original -> HF -> GGUF. This step does the
# original -> HF half using an ephemeral uv environment pinned to a transformers
# version that still bundles the Llama converter. It downloads NOTHING from
# Hugging Face (HF_HUB_OFFLINE=1); the tokenizer is built locally from
# tokenizer.model. The chat template is intentionally omitted (--instruct is the
# only part that would reach Hugging Face), because nls formats the prompt itself.
set -euo pipefail

SRC="${1:?usage: meta_to_hf.sh SRC_DIR OUT_DIR MODEL_SIZE LLAMA_VERSION [TF_VER]}"
OUT="${2:?missing OUT_DIR}"
MODEL_SIZE="${3:?missing MODEL_SIZE, e.g. 1B}"
LLAMA_VERSION="${4:?missing LLAMA_VERSION, e.g. 3.2}"
TF_VER="${5:-4.46.3}"

DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/.cache"
mkdir -p "$CACHE"

SCRIPT="$CACHE/convert_llama_weights_to_hf-$TF_VER.py"
URL="https://raw.githubusercontent.com/huggingface/transformers/v$TF_VER/src/transformers/models/llama/convert_llama_weights_to_hf.py"
if [[ ! -f "$SCRIPT" ]]; then
    echo "meta_to_hf: fetching converter (transformers v$TF_VER)" >&2
    curl -fsSL -o "$SCRIPT" "$URL"
fi

HF_HUB_OFFLINE=1 TRANSFORMERS_OFFLINE=1 uv run --no-project \
    --with torch --with "transformers==$TF_VER" --with tiktoken --with blobfile \
    --with accelerate --with sentencepiece --with safetensors \
    python "$SCRIPT" \
        --input_dir "$SRC" \
        --model_size "$MODEL_SIZE" \
        --output_dir "$OUT" \
        --llama_version "$LLAMA_VERSION" \
        --safe_serialization

echo "meta_to_hf: wrote HF-format model to $OUT" >&2
