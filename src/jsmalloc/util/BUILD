cc_library(
    name = "math",
    hdrs = ["math.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "allocable",
    hdrs = ["allocable.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "assert",
    hdrs = ["assert.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "twiddle",
    hdrs = ["twiddle.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "bitset",
    hdrs = ["bitset.h"],
    visibility = ["//visibility:public"],
    deps = [":allocable"],
)

cc_test(
    name = "bitset_test",
    srcs = ["bitset_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":bitset",
        ":allocable",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
