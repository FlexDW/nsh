#include "clean.h"

#include <cctype>

std::string trim(const std::string &s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string strip_code_fence(const std::string &s) {
    std::string t = trim(s);
    if (t.rfind("```", 0) != 0) {
        return t;
    }

    // Drop the opening fence line (which may include a language hint).
    size_t nl = t.find('\n');
    if (nl == std::string::npos) {
        // Only a fence marker, nothing usable.
        return "";
    }
    std::string inner = t.substr(nl + 1);

    // Drop a trailing closing fence if present.
    std::string trimmed = trim(inner);
    size_t last_fence = trimmed.rfind("```");
    if (last_fence != std::string::npos &&
        last_fence == trimmed.size() - 3) {
        trimmed = trimmed.substr(0, last_fence);
    }
    return trim(trimmed);
}

CleanResult clean_model_output(const std::string &raw) {
    CleanResult result;

    std::string text = strip_code_fence(trim(raw));
    text = trim(text);

    if (text.empty()) {
        result.error = "model returned empty output";
        return result;
    }

    // Take the first meaningful line.
    std::string command;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line =
            trim(text.substr(pos, nl == std::string::npos ? std::string::npos
                                                          : nl - pos));
        if (!line.empty() && line != "```") {
            command = line;
            break;
        }
        if (nl == std::string::npos) {
            break;
        }
        pos = nl + 1;
    }

    // Strip a single pair of surrounding inline-code backticks.
    if (command.size() >= 2 && command.front() == '`' && command.back() == '`') {
        command = trim(command.substr(1, command.size() - 2));
    }

    // Strip a stray leading shell prompt marker.
    if (command.rfind("$ ", 0) == 0) {
        command = trim(command.substr(2));
    }

    if (command.empty()) {
        result.error = "model returned empty output";
        return result;
    }

    result.ok = true;
    result.command = command;
    return result;
}
