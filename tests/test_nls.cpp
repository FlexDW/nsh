// Lightweight test harness for nls non-model logic.
// No external framework: asserts increment a failure counter and print.

#include <cstdio>
#include <string>
#include <vector>

#include "args.h"
#include "clean.h"

static int g_failures = 0;

static void check(bool cond, const char *what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok:   %s\n", what);
    }
}

static void check_eq(const std::string &got, const std::string &want,
                     const char *what) {
    if (got != want) {
        std::printf("FAIL: %s\n      got:  '%s'\n      want: '%s'\n", what,
                    got.c_str(), want.c_str());
        ++g_failures;
    } else {
        std::printf("ok:   %s\n", what);
    }
}

static void test_trim() {
    check_eq(trim("  hi  "), "hi", "trim spaces");
    check_eq(trim("\n\t hi \r\n"), "hi", "trim mixed whitespace");
    check_eq(trim(""), "", "trim empty");
    check_eq(trim("no-trim"), "no-trim", "trim none");
}

static void test_fence_stripping() {
    check_eq(strip_code_fence("```\nls -la\n```"), "ls -la",
             "fence plain");
    check_eq(strip_code_fence("```sh\nls -la\n```"), "ls -la",
             "fence with lang");
    check_eq(strip_code_fence("```bash\nrg -F 'x('\n```"), "rg -F 'x('",
             "fence bash preserves parens");
    check_eq(strip_code_fence("ls -la"), "ls -la", "no fence");
    check_eq(strip_code_fence("```zsh\nls -la"), "ls -la",
             "fence without closing");
}

static void test_clean_output() {
    CleanResult r1 = clean_model_output("  git status  ");
    check(r1.ok && r1.command == "git status", "clean trims plain");

    CleanResult r2 = clean_model_output("```sh\nrg -l -F 'triggerOp(' .\n```");
    check(r2.ok && r2.command == "rg -l -F 'triggerOp(' .",
          "clean strips fence and preserves literal paren");

    CleanResult r3 = clean_model_output("");
    check(!r3.ok, "clean rejects empty");

    CleanResult r4 = clean_model_output("   \n  \n ");
    check(!r4.ok, "clean rejects whitespace-only");

    CleanResult r5 = clean_model_output("$ ls -la\n");
    check(r5.ok && r5.command == "ls -la", "clean strips leading prompt marker");

    CleanResult r6 = clean_model_output("git log main..HEAD --oneline\nThis "
                                        "shows the commits.");
    check(r6.ok && r6.command == "git log main..HEAD --oneline",
          "clean takes first line only");

    CleanResult r7 = clean_model_output("`find . -name '*.py' | wc -l`");
    check(r7.ok && r7.command == "find . -name '*.py' | wc -l",
          "clean strips surrounding inline backticks");
}

static void test_arg_parsing() {
    ParsedArgs a = parse_args({});
    check(a.mode == ParsedArgs::Mode::Error, "no args -> error");

    ParsedArgs h = parse_args({"--help"});
    check(h.mode == ParsedArgs::Mode::Help, "--help");
    ParsedArgs h2 = parse_args({"-h"});
    check(h2.mode == ParsedArgs::Mode::Help, "-h");

    ParsedArgs v = parse_args({"--version"});
    check(v.mode == ParsedArgs::Mode::Version, "--version");
    ParsedArgs v2 = parse_args({"-V"});
    check(v2.mode == ParsedArgs::Mode::Version, "-V");

    ParsedArgs one = parse_args({"find files containing triggerOp("});
    check(one.mode == ParsedArgs::Mode::Request &&
              one.request == "find files containing triggerOp(",
          "single quoted request");

    ParsedArgs many = parse_args({"find", "files", "containing", "triggerOp("});
    check(many.mode == ParsedArgs::Mode::Request &&
              many.request == "find files containing triggerOp(",
          "multi-arg request joined");

    ParsedArgs dd = parse_args({"--", "--version"});
    check(dd.mode == ParsedArgs::Mode::Request && dd.request == "--version",
          "-- treats following flags as request");

    ParsedArgs dd2 = parse_args({"--", "foo", "&&", "bar"});
    check(dd2.mode == ParsedArgs::Mode::Request &&
              dd2.request == "foo && bar",
          "-- preserves shell metacharacters");
}

int main() {
    test_trim();
    test_fence_stripping();
    test_clean_output();
    test_arg_parsing();

    if (g_failures == 0) {
        std::printf("\nAll tests passed.\n");
        return 0;
    }
    std::printf("\n%d test(s) failed.\n", g_failures);
    return 1;
}
