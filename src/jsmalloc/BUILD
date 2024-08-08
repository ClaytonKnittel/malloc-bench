cc_library(
    name = "block",
    hdrs = ["block.h"],
    srcs = ["block.cc"],
    deps = [
        ":mallocator",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
    ],
)
cc_library(
    name = "mallocator",
    hdrs = ["mallocator.h"],
    deps = [
        "//src:heap_interface",
        "//src/jsmalloc/util:assert",
    ],
)

cc_test(
    name = "block_test",
    srcs = ["block_test.cc"],
    deps = [
        ":block",
        ":mallocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
cc_library(
    name = "jsmalloc",
    hdrs = ["jsmalloc.h"],
    srcs = ["jsmalloc.cc"],
    deps = [
        ":block",
        ":mallocator",
        "//src:heap_interface",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/collections:intrusive_linked_list",
    ],
    visibility = ["//visibility:public"],
)
