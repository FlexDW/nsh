#ifndef NLS_ARGS_H
#define NLS_ARGS_H

#include <string>
#include <vector>

struct ParsedArgs {
    enum class Mode { Request, Help, Version, Error };
    Mode mode = Mode::Error;
    std::string request;
    std::string error;
};

// Parse the command-line arguments (excluding argv[0]).
// Everything after a literal "--" is treated as the request verbatim.
// Otherwise a leading --help/-h or --version/-V is recognized, and any
// remaining arguments are joined with single spaces to form the request.
ParsedArgs parse_args(const std::vector<std::string> &args);

#endif // NLS_ARGS_H
