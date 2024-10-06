cc_library(
    name = "allocator",
    hdrs = ["allocator.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@cc-util//util:absl_util",
        "@abseil-cpp//absl/strings:str_format",
        "//src:heap_factory",
        "//src:heap_interface",
        "//src/jsmalloc/util:file_logger",
        "//src/jsmalloc/util:assert",
    ],
)

cc_library(
    name = "jsmalloc",
    srcs = ["jsmalloc.cc"],
    hdrs = ["jsmalloc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":allocator",
        "//src:heap_factory",
        "//src:heap_interface",
        "//src/jsmalloc/blocks:block",
        "//src/jsmalloc/blocks:free_block_allocator",
        "//src/jsmalloc/blocks:large_block_allocator",
        "//src/jsmalloc/blocks:sentinel_block_allocator",
        "//src/jsmalloc/blocks:small_block_allocator",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/util:file_logger",
        "//src/jsmalloc/util:twiddle",
    ],
)
