package(default_visibility = ["//src:__subpackages__"])

cc_library(
    name = "allocated_block",
    srcs = ["allocated_block.cc"],
    hdrs = ["allocated_block.h"],
    deps = [
        ":block",
        ":free_block",
        "//src:heap_factory",
        "//src:heap_interface",
    ],
)

cc_library(
    name = "block",
    srcs = ["block.cc"],
    hdrs = ["block.h"],
    deps = [
        ":malloc_assert",
        "//src:heap_factory",
        "//src:heap_interface",
    ],
)

cc_library(
    name = "free_block",
    srcs = ["free_block.cc"],
    hdrs = ["free_block.h"],
    deps = [
        ":block",
    ],
)

cc_library(
    name = "free_list",
    srcs = ["free_block.cc"],
    hdrs = ["free_list.h"],
    deps = [
        ":allocated_block",
        ":block",
        ":free_block",
        "//src:heap_factory",
        "//src:heap_interface",
    ],
)

cc_library(
    name = "global_state",
    srcs = ["global_state.cc"],
    hdrs = ["global_state.h"],
    deps = [
        ":free_block",
        "//src:heap_factory",
        "//src:heap_interface",
    ],
)

cc_library(
    name = "heap_check",
    srcs = ["heap_check.cc"],
    hdrs = ["heap_check.h"],
)

cc_library(
    name = "malloc_assert",
    hdrs = ["malloc_assert.h"],
)
