#ifndef NSH_ARGS_H
#define NSH_ARGS_H

#include <string>
#include <vector>

struct ParsedArgs {
    enum class Mode { Request, Error };
    Mode mode = Mode::Error;
    std::string request;
    std::string error;
};

// Parse the command-line arguments (excluding argv[0]).
// Everything after a literal "--" is treated as the request verbatim.
// Otherwise all arguments are joined with single spaces to form the request.
ParsedArgs parse_args(const std::vector<std::string> &args);

#endif // NSH_ARGS_H
