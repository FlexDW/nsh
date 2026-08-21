#ifndef NLS_CLEAN_H
#define NLS_CLEAN_H

#include <string>

// Remove leading and trailing ASCII whitespace.
std::string trim(const std::string &s);

// If the text is wrapped in a single Markdown code fence (```lang ... ```),
// return the inner content. Otherwise return the trimmed text unchanged.
std::string strip_code_fence(const std::string &s);

struct CleanResult {
    bool ok = false;
    std::string command;
    std::string error;
};

// Normalize raw model output into a single shell command.
// Strips whitespace, a surrounding code fence, and a leading "$ " prompt,
// then returns the first non-empty line. Empty output is rejected.
CleanResult clean_model_output(const std::string &raw);

#endif // NLS_CLEAN_H
