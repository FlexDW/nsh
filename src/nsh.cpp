// nsh - natural-language shell-command translator
//
// nsh generates text. It never executes the generated command.
//
// Inference runs by invoking the `apfel` executable (Apple Intelligence from
// the command line) as a subprocess. The subprocess boundary is isolated in
// run_inference() so a different backend (another local model, a cloud API)
// can replace it later without touching argument parsing, prompt building, or
// output cleanup.

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "args.h"
#include "clean.h"

namespace {

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

// Returns "" when missing; the caller reports the error.
std::string load_system_prompt() {
    std::string home = env_or("HOME", "");
    if (home.empty()) {
        return "";
    }
    std::string path = home + "/.nsh/system.txt";
    if (!file_readable(path)) {
        return "";
    }
    return read_file(path);
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
    std::string explicit_path = env_or("NSH_APFEL", "");
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
// This is the only subprocess nsh ever launches. It is executed via
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
    argv.push_back(env_or("NSH_TEMPERATURE", "0"));
    argv.push_back("--max-tokens");
    argv.push_back(env_or("NSH_MAX_TOKENS", "150"));
    argv.push_back("-s");
    argv.push_back(system_prompt);

    for (const std::string &extra :
         split_whitespace(env_or("NSH_APFEL_ARGS", ""))) {
        argv.push_back(extra);
    }

    // "--" ends options; the request is a single positional argument, so a
    // request beginning with "-" is never treated as a flag.
    argv.push_back("--");
    argv.push_back(request);

    bool debug = !env_or("NSH_DEBUG", "").empty();

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
                           " (set NSH_DEBUG=1 to see its output)";
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

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    ParsedArgs parsed = parse_args(args);

    switch (parsed.mode) {
    case ParsedArgs::Mode::Error:
        std::fprintf(stderr, "nsh: %s\n", parsed.error.c_str());
        std::fprintf(stderr, "nsh: usage: nsh <request>\n");
        return 2;
    case ParsedArgs::Mode::Request:
        break;
    }

    std::string runner = resolve_runner();
    if (runner.empty()) {
        std::fprintf(stderr, "nsh: apfel not found\n");
        std::fprintf(stderr,
                     "nsh: install apfel (e.g. 'brew install apfel') "
                     "or set NSH_APFEL=/path/to/apfel\n");
        return 4;
    }

    std::string system_prompt = load_system_prompt();
    if (system_prompt.empty()) {
        std::fprintf(stderr, "nsh: no system prompt found\n");
        std::fprintf(stderr,
                     "nsh: expected ~/.nsh/system.txt (run the installer)\n");
        return 3;
    }

    InferenceResult inf = run_inference(
        runner, build_system_prompt(system_prompt), parsed.request);
    if (!inf.ok) {
        std::fprintf(stderr, "nsh: %s\n", inf.error.c_str());
        return 5;
    }

    CleanResult cleaned = clean_model_output(inf.output);
    if (!cleaned.ok) {
        std::fprintf(stderr, "nsh: %s\n", cleaned.error.c_str());
        return 6;
    }

    std::printf("%s\n", cleaned.command.c_str());
    return 0;
}
