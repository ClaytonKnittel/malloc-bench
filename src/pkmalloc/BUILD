cc_library(
    name = "allocated_block",
    srcs = ["allocated_block.cc"],
    hdrs = ["allocated_block.h"],
    deps = [
        ":block",
        ":malloc_assert",
    ],
)

cc_library(
    name = "block",
    srcs = ["block.cc"],
    hdrs = ["block.h"],
    deps = [
        ":malloc_assert",
        "//src:singleton_heap",
    ],
)

cc_library(
    name = "free_block",
    srcs = ["free_block.cc"],
    hdrs = ["free_block.h"],
    deps = [
        ":block",
        ":malloc_assert",
    ],
)

cc_library(
    name = "heap_check",
    srcs = ["heap_check.cc"],
    hdrs = ["fheap_check.h"],
)

cc_library(
    name = "malloc_assert",
    hdrs = ["malloc_assert.h"],
)

cc_library(
    name = "metdata",
    srcs = ["metdata.cc"],
    hdrs = ["metdata.h"],
    deps = [
        ":allocated_block",
        ":block",
        ":free_block",
    ],
)
