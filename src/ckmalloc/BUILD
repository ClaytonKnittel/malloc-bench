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
    name = "heap_manager",
    srcs = ["heap_manager.cc"],
    hdrs = ["heap_manager.h"],
    deps = [
        ":slab_manager",
        ":util",
    ],
)

cc_library(
    name = "red_black_tree",
    srcs = ["red_black_tree.cc"],
    hdrs = ["red_black_tree.h"],
    deps = [
        ":util",
    ],
)

cc_test(
    name = "red_black_tree_test",
    srcs = ["red_black_tree_test.cc"],
    deps = [
        ":red_black_tree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "slab",
    srcs = ["slab.cc"],
    hdrs = ["slab.h"],
    deps = [
    ],
)

cc_library(
    name = "slab_id",
    hdrs = ["slab_id.h"],
    deps = [
    ],
)

cc_library(
    name = "slab_manager",
    srcs = ["slab_manager.cc"],
    hdrs = ["slab_manager.h"],
    deps = [
        "//src:heap_interface",
        ":slab_id",
        ":util",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
    ],
)
