#!/usr/bin/env python3
"""Build a tiny, untrained Llama model fully offline, for pipeline testing.

This downloads NOTHING. It trains a small SentencePiece tokenizer on a local
text sample and random-initializes a small LlamaForCausalLM, then saves it as a
HuggingFace-format directory that convert_to_gguf.py can consume.

The resulting model produces gibberish (weights are random) -- its only purpose
is to prove the convert -> gguf -> llama-cli -> nls path works without needing
any real weights. Swap in real Llama weights for sensible output.

Example:
    uv run python make_test_model.py --out ./.cache/tiny-llama
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

CORPUS = """
find all files containing triggerOp in the current directory
list the twenty largest files under here sorted by size
show commits on this branch that are not on main
search recursively for a literal string but ignore node_modules
grep for unsafe in rust files modified in the last week
print the current working directory and its disk usage
count the number of lines in every python file
remove trailing whitespace from every markdown file
copy the config file into the backup directory
rename every jpeg file to lowercase
""".strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, help="output model directory")
    parser.add_argument("--vocab-size", type=int, default=512)
    args = parser.parse_args()

    import sentencepiece as spm
    import torch
    from transformers import LlamaConfig, LlamaForCausalLM

    out = Path(args.out).expanduser()
    out.mkdir(parents=True, exist_ok=True)

    corpus_path = out / "corpus.txt"
    corpus_path.write_text(CORPUS + "\n", encoding="utf-8")

    # Train a tiny SentencePiece tokenizer (unk=0, bos=1, eos=2 -> Llama-style).
    spm.SentencePieceTrainer.train(
        input=str(corpus_path),
        model_prefix=str(out / "tokenizer"),
        vocab_size=args.vocab_size,
        model_type="bpe",
        character_coverage=1.0,
        hard_vocab_limit=False,
        bos_id=1,
        eos_id=2,
        unk_id=0,
        pad_id=-1,
    )
    (out / "tokenizer.model").write_bytes((out / "tokenizer.model").read_bytes())

    sp = spm.SentencePieceProcessor(model_file=str(out / "tokenizer.model"))
    vocab_size = sp.get_piece_size()

    config = LlamaConfig(
        vocab_size=vocab_size,
        hidden_size=64,
        intermediate_size=128,
        num_hidden_layers=2,
        num_attention_heads=4,
        num_key_value_heads=4,
        max_position_embeddings=512,
        bos_token_id=1,
        eos_token_id=2,
    )
    torch.manual_seed(0)
    model = LlamaForCausalLM(config)
    model.save_pretrained(out)

    print(f"wrote tiny model to {out} (vocab_size={vocab_size})", file=sys.stderr)
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
