# nls - natural-language shell-command translator
# Simple Makefile for a small native CLI. No external build deps beyond a
# C++17 compiler; llama.cpp is used at runtime via the llama-cli executable.

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
PREFIX   ?= $(HOME)/.local

BUILD := build
SRC   := src

CORE_SRCS := $(SRC)/clean.cpp $(SRC)/args.cpp
HEADERS   := $(SRC)/clean.h $(SRC)/args.h

# Model conversion (convert/ subfolder, managed by uv).
MODEL_OUT  ?= $(HOME)/.local/share/nls/model.gguf
OUTTYPE    ?= f16
QUANTIZE   ?= Q4_K_M
META_MODEL ?= Llama-3.2-1B-Instruct
MODEL_SIZE ?= 1B
LLAMA_VERSION ?= 3.2

.PHONY: all test clean install uninstall convert-setup model model-meta

all: $(BUILD)/nls

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/nls: $(SRC)/nls.cpp $(CORE_SRCS) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)/nls.cpp $(CORE_SRCS)

$(BUILD)/nls_tests: tests/test_nls.cpp $(CORE_SRCS) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(SRC) -o $@ tests/test_nls.cpp $(CORE_SRCS)

test: $(BUILD)/nls_tests
	./$(BUILD)/nls_tests

install: $(BUILD)/nls
	install -d "$(PREFIX)/bin" "$(PREFIX)/share/nls"
	install -m 0755 $(BUILD)/nls "$(PREFIX)/bin/nls"
	install -m 0644 prompts/system.txt "$(PREFIX)/share/nls/system.txt"
	install -m 0644 shell/nls.zsh "$(PREFIX)/share/nls/nls.zsh"
	@echo
	@echo "Installed nls to $(PREFIX)/bin/nls"
	@echo "Add the zsh integration with:"
	@echo "  source \"$(PREFIX)/share/nls/nls.zsh\""

uninstall:
	rm -f "$(PREFIX)/bin/nls"
	rm -rf "$(PREFIX)/share/nls"

# Create the Python venv (latest Python via uv) and install PyTorch + the
# pinned llama.cpp converter. Run once before `make model`.
convert-setup:
	cd convert && uv sync

# Convert a local model directory into a GGUF for nls.
# SRC_MODEL may be an HF-format dir (config.json + weights). Example:
#   make model SRC_MODEL=/path/to/Llama-3.2-3B-Instruct-hf
model:
	@test -n "$(SRC_MODEL)" || { echo "error: set SRC_MODEL=/path/to/hf-model-dir" >&2; exit 2; }
	cd convert && uv run python convert_to_gguf.py \
		--src "$(abspath $(SRC_MODEL))" \
		--out "$(MODEL_OUT)" \
		--outtype "$(OUTTYPE)" \
		--quantize "$(QUANTIZE)"
	@echo
	@echo "Wrote $(MODEL_OUT)"
	@echo "Point nls at it with:  export NLS_MODEL=\"$(MODEL_OUT)\""

# Download Llama original weights from Meta's CDN (no Hugging Face) and convert
# all the way to a quantized GGUF. Accept the license at
# https://www.llama.com/llama-downloads to get a signed URL, then either export
# LLAMA_SIGNED_URL or paste it at the hidden prompt.
#   make model-meta META_MODEL=Llama-3.2-1B-Instruct MODEL_SIZE=1B
#   make model-meta META_MODEL=Llama-3.2-3B-Instruct MODEL_SIZE=3B
# Llama 3.x needs an original -> HF -> GGUF path (tiktoken tokenizer), done here
# fully offline.
model-meta:
	cd convert && uv run python download_meta.py \
		--model-path "$(META_MODEL)" \
		--dest "weights/$(META_MODEL)"
	cd convert && bash ./meta_to_hf.sh \
		"weights/$(META_MODEL)" "weights/$(META_MODEL)-hf" \
		"$(MODEL_SIZE)" "$(LLAMA_VERSION)"
	$(MAKE) model SRC_MODEL="convert/weights/$(META_MODEL)-hf"

clean:
	rm -rf $(BUILD)
