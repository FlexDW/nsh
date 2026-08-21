// nls - natural-language shell-command translator
//
// nls generates text. It never executes the generated command.
//
// Inference runs by invoking the `apfel` executable (Apple Intelligence from
// the command line) as a subprocess. The subprocess boundary is isolated in
// run_inference() so a different backend (another local model, a cloud API)
// can replace it later without touching argument parsing, prompt building, or
// output cleanup.

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "args.h"
#include "clean.h"

#ifndef NLS_VERSION
#define NLS_VERSION "0.1.0"
#endif

namespace {

// The compiled-in fallback keeps nls working even if the prompt file cannot
// be located. It mirrors prompts/system.txt.
const char *kDefaultSystemPrompt =
    "You translate natural-language requests into a single shell command.\n"
    "\n"
    "Environment:\n"
    "- operating system: macOS (BSD userland, not GNU/Linux)\n"
    "- shell: zsh\n"
    "- preferred utilities: rg, fd, jq, yq, git, find, grep, sed, awk\n"
    "\n"
    "Rules:\n"
    "- Return exactly one shell command.\n"
    "- Output only the command, on a single line.\n"
    "- Do not use Markdown or code fences.\n"
    "- Do not explain the command.\n"
    "- Do not add a leading prompt marker such as \"$\".\n"
    "- Never execute anything.\n"
    "- Prefer non-destructive commands.\n"
    "- Prefer modern installed utilities such as rg and fd when appropriate.\n"
    "- Quote arguments correctly for zsh.\n"
    "- Preserve literal special characters such as (, ), [, ], *, $, and "
    "quotes correctly.\n"
    "- When matching a literal string that contains regex or glob "
    "metacharacters, use a fixed-string search (for example rg -F).\n"
    "- Use BSD-compatible flags for macOS. Avoid GNU-only options such as "
    "stat -c, date -d, find -printf, or grep -P. For file sizes use du or "
    "stat -f%z.\n"
    "- Build one correct top-level pipeline. Do not put a sort or head "
    "pipeline inside a per-file -exec that only sees one file at a time.\n"
    "- Do not invent filenames, paths, branches, namespaces, hosts, or other "
    "values unless clearly implied.\n"
    "- If a pipeline is appropriate, return it as one shell command.\n"
    "\n"
    "Examples:\n"
    "Request: find all files containing triggerOp(\n"
    "Command: rg -l -F 'triggerOp(' .\n"
    "Request: find recursively for triggerOp( but ignore node_modules\n"
    "Command: rg -l -F 'triggerOp(' . -g '!node_modules/**'\n"
    "Request: show commits on this branch that aren't on main\n"
    "Command: git log main..HEAD --oneline\n"
    "Request: show the 20 largest files under here\n"
    "Command: find . -type f -exec du -h {} + | sort -rh | head -n 20\n"
    "Request: list files modified in the last 7 days\n"
    "Command: find . -type f -mtime -7\n"
    "Request: count lines in all python files\n"
    "Command: find . -name '*.py' -type f -print0 | xargs -0 wc -l\n";

std::string env_or(const char *name, const std::string &fallback) {
    const char *v = std::getenv(name);
    if (v && *v) {
        return std::string(v);
    }
    return fallback;
}

bool file_readable(const std::string &path) {
    return !path.empty() && access(path.c_str(), R_OK) == 0;
}

bool file_executable(const std::string &path) {
    return !path.empty() && access(path.c_str(), X_OK) == 0;
}

std::string read_file(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return "";
    }
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        content.append(buf, n);
    }
    std::fclose(f);
    return content;
}

std::string executable_dir() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string raw(size, '\0');
    if (_NSGetExecutablePath(&raw[0], &size) != 0) {
        return "";
    }
    // Resolve symlinks and relative components.
    char resolved[PATH_MAX];
    if (realpath(raw.c_str(), resolved) == nullptr) {
        return "";
    }
    std::string path(resolved);
    // dirname() may modify its argument.
    std::vector<char> tmp(path.begin(), path.end());
    tmp.push_back('\0');
    return std::string(dirname(tmp.data()));
}

std::string load_system_prompt() {
    // 1. Inline override.
    std::string inline_prompt = env_or("NLS_PROMPT", "");
    if (!inline_prompt.empty()) {
        return inline_prompt;
    }

    // 2. File override.
    std::string prompt_file = env_or("NLS_SYSTEM_PROMPT", "");
    if (file_readable(prompt_file)) {
        return read_file(prompt_file);
    }

    // 3./4./5. Locations relative to the executable.
    std::string dir = executable_dir();
    if (!dir.empty()) {
        const char *candidates[] = {
            "/../share/nls/system.txt",
            "/../prompts/system.txt",
            "/prompts/system.txt",
        };
        for (const char *rel : candidates) {
            std::string p = dir + rel;
            if (file_readable(p)) {
                return read_file(p);
            }
        }
    }

    // 6. Compiled-in fallback.
    return kDefaultSystemPrompt;
}

std::string get_cwd() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "unknown";
}

std::string get_arch() {
    struct utsname u;
    if (uname(&u) == 0) {
        return std::string(u.machine);
    }
    return "unknown";
}

// apfel handles the chat template and returns just the completion, so nls
// only needs to append runtime context to the system prompt. The request is
// passed to apfel separately as the user message.
std::string build_system_prompt(const std::string &system_prompt) {
    std::string sys = system_prompt;
    if (!sys.empty() && sys.back() != '\n') {
        sys += '\n';
    }
    sys += "\nContext:\n";
    sys += "cwd: " + get_cwd() + "\n";
    sys += "shell: zsh\n";
    sys += "os: macOS\n";
    sys += "architecture: " + get_arch() + "\n";
    return sys;
}

// Resolve the apfel executable: explicit env, then PATH search.
std::string resolve_runner() {
    std::string explicit_path = env_or("NLS_APFEL", "");
    if (!explicit_path.empty()) {
        return file_executable(explicit_path) ? explicit_path : "";
    }

    const char *path_env = std::getenv("PATH");
    if (!path_env) {
        return "";
    }
    std::string path(path_env);
    size_t start = 0;
    while (start <= path.size()) {
        size_t colon = path.find(':', start);
        std::string dir =
            path.substr(start, colon == std::string::npos ? std::string::npos
                                                          : colon - start);
        if (!dir.empty()) {
            std::string candidate = dir + "/apfel";
            if (file_executable(candidate)) {
                return candidate;
            }
        }
        if (colon == std::string::npos) {
            break;
        }
        start = colon + 1;
    }
    return "";
}

std::vector<std::string> split_whitespace(const std::string &s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() &&
               std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        size_t start = i;
        while (i < s.size() &&
               !std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        if (i > start) {
            out.push_back(s.substr(start, i - start));
        }
    }
    return out;
}

struct InferenceResult {
    bool ok = false;
    std::string output;
    std::string error;
};

// Run apfel as a subprocess and capture its stdout.
// This is the only subprocess nls ever launches. It is executed via
// fork/execv with an explicit argument vector (no shell), so the user's
// request is never interpreted by a shell.
InferenceResult run_inference(const std::string &runner,
                              const std::string &system_prompt,
                              const std::string &request) {
    InferenceResult result;

    std::vector<std::string> argv;
    argv.push_back(runner);
    argv.push_back("-q");
    argv.push_back("--temperature");
    argv.push_back(env_or("NLS_TEMPERATURE", "0"));
    argv.push_back("--max-tokens");
    argv.push_back(env_or("NLS_MAX_TOKENS", "150"));
    argv.push_back("-s");
    argv.push_back(system_prompt);

    for (const std::string &extra :
         split_whitespace(env_or("NLS_APFEL_ARGS", ""))) {
        argv.push_back(extra);
    }

    // "--" ends options; the request is a single positional argument, so a
    // request beginning with "-" is never treated as a flag.
    argv.push_back("--");
    argv.push_back(request);

    bool debug = !env_or("NLS_DEBUG", "").empty();

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.error = std::string("failed to create pipe: ") +
                       std::strerror(errno);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.error =
            std::string("failed to fork: ") + std::strerror(errno);
        return result;
    }

    if (pid == 0) {
        // Child.
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        // Never let the runner block on interactive input.
        int devnull_in = open("/dev/null", O_RDONLY);
        if (devnull_in >= 0) {
            dup2(devnull_in, STDIN_FILENO);
            close(devnull_in);
        }

        if (!debug) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
        }

        std::vector<char *> cargv;
        cargv.reserve(argv.size() + 1);
        for (auto &a : argv) {
            cargv.push_back(const_cast<char *>(a.c_str()));
        }
        cargv.push_back(nullptr);

        execv(runner.c_str(), cargv.data());
        // execv only returns on failure.
        _exit(127);
    }

    // Parent.
    close(pipefd[1]);
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) < 0) {
        result.error =
            std::string("failed to wait for apfel: ") +
            std::strerror(errno);
        return result;
    }

    if (WIFEXITED(wstatus)) {
        int code = WEXITSTATUS(wstatus);
        if (code == 127) {
            result.error = "failed to launch apfel (exec error)";
            return result;
        }
        if (code != 0) {
            result.error = "apfel exited with status " +
                           std::to_string(code) +
                           " (set NLS_DEBUG=1 to see its output)";
            return result;
        }
    } else {
        result.error = "apfel terminated abnormally";
        return result;
    }

    result.ok = true;
    result.output = output;
    return result;
}

void print_help() {
    std::printf(
        "nls - translate natural language into a shell command (local, "
        "offline)\n"
        "\n"
        "Usage:\n"
        "  nls \"find files containing triggerOp(\"\n"
        "  nls find files containing triggerOp\\(\n"
        "  nls -- <request>      treat everything after -- as the request\n"
        "\n"
        "Options:\n"
        "  -h, --help            show this help and exit\n"
        "  -V, --version         show version and exit\n"
        "\n"
        "nls prints the generated command to stdout and never executes it.\n"
        "\n"
        "Environment:\n"
        "  NLS_APFEL             path to apfel (default: found on PATH)\n"
        "  NLS_SYSTEM_PROMPT     path to a system prompt file (optional)\n"
        "  NLS_PROMPT            inline system prompt text (optional)\n"
        "  NLS_MAX_TOKENS        max output tokens (default: 150)\n"
        "  NLS_TEMPERATURE       sampling temperature, 0 = deterministic "
        "(default: 0)\n"
        "  NLS_APFEL_ARGS        extra args passed to apfel (optional)\n"
        "  NLS_DEBUG             if set, show apfel diagnostics\n");
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    ParsedArgs parsed = parse_args(args);

    switch (parsed.mode) {
    case ParsedArgs::Mode::Help:
        print_help();
        return 0;
    case ParsedArgs::Mode::Version:
        std::printf("nls %s\n", NLS_VERSION);
        return 0;
    case ParsedArgs::Mode::Error:
        std::fprintf(stderr, "nls: %s\n", parsed.error.c_str());
        std::fprintf(stderr, "nls: try 'nls --help'\n");
        return 2;
    case ParsedArgs::Mode::Request:
        break;
    }

    std::string runner = resolve_runner();
    if (runner.empty()) {
        std::fprintf(stderr, "nls: apfel not found\n");
        std::fprintf(stderr,
                     "nls: install apfel (e.g. 'brew install apfel') "
                     "or set NLS_APFEL=/path/to/apfel\n");
        return 4;
    }

    std::string system_prompt = build_system_prompt(load_system_prompt());

    InferenceResult inf = run_inference(runner, system_prompt, parsed.request);
    if (!inf.ok) {
        std::fprintf(stderr, "nls: %s\n", inf.error.c_str());
        return 5;
    }

    CleanResult cleaned = clean_model_output(inf.output);
    if (!cleaned.ok) {
        std::fprintf(stderr, "nls: %s\n", cleaned.error.c_str());
        return 6;
    }

    std::printf("%s\n", cleaned.command.c_str());
    return 0;
}
