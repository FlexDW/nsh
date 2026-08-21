#include "args.h"

#include "clean.h"

namespace {

std::string join_with_spaces(const std::vector<std::string> &parts,
                             size_t start) {
    std::string out;
    for (size_t i = start; i < parts.size(); ++i) {
        if (i > start) {
            out += ' ';
        }
        out += parts[i];
    }
    return out;
}

} // namespace

ParsedArgs parse_args(const std::vector<std::string> &args) {
    ParsedArgs result;

    if (args.empty()) {
        result.mode = ParsedArgs::Mode::Error;
        result.error = "no request provided";
        return result;
    }

    // Everything after "--" is the request, verbatim.
    if (args[0] == "--") {
        std::string request = trim(join_with_spaces(args, 1));
        if (request.empty()) {
            result.mode = ParsedArgs::Mode::Error;
            result.error = "no request provided";
            return result;
        }
        result.mode = ParsedArgs::Mode::Request;
        result.request = request;
        return result;
    }

    std::string request = trim(join_with_spaces(args, 0));
    if (request.empty()) {
        result.mode = ParsedArgs::Mode::Error;
        result.error = "no request provided";
        return result;
    }

    result.mode = ParsedArgs::Mode::Request;
    result.request = request;
    return result;
}
