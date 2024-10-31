licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "double-conversion",
    srcs = glob(["double-conversion/*.cc"]),
    hdrs = glob(["double-conversion/*.h"]),
    includes = [
        ".",
    ],
    linkopts = [
        "-lm",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "cctest",
    srcs = glob([
        "test/cctest/*.cc",
        "test/cctest/*.h",
    ]),
    args = [
        "test-bignum",
        "test-bignum-dtoa",
        "test-conversions",
        "test-dtoa",
        "test-fast-dtoa",
        "test-fixed-dtoa",
        "test-ieee",
        "test-strtod",
    ],
    visibility = ["//visibility:public"],
    deps = [":double-conversion"],
)
