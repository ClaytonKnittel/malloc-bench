cc_library(
    name = "ckmalloc",
    srcs = ["ckmalloc.cc"],
    hdrs = ["ckmalloc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":allocator",
        "//src:singleton_heap",
    ],
)

cc_library(
    name = "allocator",
    srcs = ["allocator.cc"],
    hdrs = ["allocator.h"],
    deps = [
        ":util",
        "//src:heap_interface",
    ],
)

cc_test(
    name = "allocator_test",
    srcs = ["allocator_test.cc"],
    deps = [
        ":allocator",
        ":util",
        "//src:sim_heap",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
    ],
)
