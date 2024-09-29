cc_library(
    name = "block",
    srcs = ["block.cc"],
    hdrs = ["block.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "block_test",
    srcs = ["block_test.cc"],
    deps = [
        ":block",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "free_block",
    srcs = ["free_block.cc"],
    hdrs = ["free_block.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/collections:rbtree",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "free_block_test",
    srcs = ["free_block_test.cc"],
    deps = [
        ":free_block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "large_block",
    srcs = ["large_block.cc"],
    hdrs = ["large_block.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":free_block",
        "//src/jsmalloc/util:twiddle",
        ":free_block_allocator",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
    ],
)

cc_test(
    name = "large_block_test",
    srcs = ["large_block_test.cc"],
    deps = [
        ":block",
        ":free_block_allocator",
        ":large_block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "large_block_allocator",
    srcs = ["large_block_allocator.cc"],
    hdrs = ["large_block_allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":free_block",
        ":free_block_allocator",
        ":large_block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/collections:rbtree",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "large_block_allocator_test",
    srcs = ["large_block_allocator_test.cc"],
    deps = [
        ":block",
        ":large_block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "small_block",
    srcs = ["small_block.cc"],
    hdrs = ["small_block.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":free_block",
        ":free_block_allocator",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:bitset",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "small_block_test",
    srcs = ["small_block_test.cc"],
    deps = [
        ":block",
        ":free_block_allocator",
        ":sentinel_block_allocator",
        ":small_block",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "small_block_allocator",
    srcs = ["small_block_allocator.cc"],
    hdrs = ["small_block_allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":fixed_size_free_block_allocator",
        ":free_block",
        ":free_block_allocator",
        ":sentinel_block_allocator",
        ":small_block",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
    ],
)

cc_test(
    name = "small_block_allocator_test",
    srcs = ["small_block_allocator_test.cc"],
    deps = [
        ":small_block_allocator",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "free_block_allocator",
    srcs = ["free_block_allocator.cc"],
    hdrs = ["free_block_allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":free_block",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/blocks/freelists:small_size_free_list",
        "//src/jsmalloc/blocks/freelists:learned_size_free_list",
        "//src/jsmalloc/blocks/freelists:rbtree_free_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:bitset",
        "//src/jsmalloc/util:math",
        "@abseil-cpp//absl/base:prefetch",
    ],
)

cc_test(
    name = "free_block_allocator_test",
    srcs = ["free_block_allocator_test.cc"],
    deps = [
        ":block",
        ":free_block",
        ":free_block_allocator",
        ":sentinel_block_allocator",
        "//src/jsmalloc:allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "fixed_size_free_block_allocator",
    hdrs = ["fixed_size_free_block_allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/collections:intrusive_stack",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_library(
    name = "sentinel_block_allocator",
    hdrs = ["sentinel_block_allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        "//src/jsmalloc:allocator",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
    ],
)
