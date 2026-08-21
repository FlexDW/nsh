# nsh - natural-language shell-command translator
# Simple Makefile for a small native CLI. Build dep: a C++17 compiler (ships
# with the Xcode Command Line Tools). apfel is used at runtime.

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

BUILD := build
SRC   := src

CORE_SRCS := $(SRC)/clean.cpp $(SRC)/args.cpp
HEADERS   := $(SRC)/clean.h $(SRC)/args.h

.PHONY: all test clean dist

all: $(BUILD)/nsh

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/nsh: $(SRC)/nsh.cpp $(CORE_SRCS) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)/nsh.cpp $(CORE_SRCS)

$(BUILD)/nsh_tests: tests/test_nsh.cpp $(CORE_SRCS) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(SRC) -o $@ tests/test_nsh.cpp $(CORE_SRCS)

test: $(BUILD)/nsh_tests
	./$(BUILD)/nsh_tests

# Stage the built binary as the release asset that is committed to the repo and
# published by .github/workflows/release.yml.
dist: $(BUILD)/nsh
	install -d dist
	install -m 0755 $(BUILD)/nsh dist/nsh-macos-arm64
	@echo "Wrote dist/nsh-macos-arm64"

clean:
	rm -rf $(BUILD)
