load("@buildifier_prebuilt//:rules.bzl", "buildifier")
load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")

buildifier(
    name = "buildifier",
    exclude_patterns = [
        "./.git/*",
    ],
    lint_mode = "fix",
    mode = "fix",
)

refresh_compile_commands(
    name = "refresh_compile_commands",
    targets = {
        "//...": "--process_headers_in_dependencies=false --features=-parse_headers",
    },
)
