cc_library(
    name = "chunky_block",
    hdrs = ["chunky_block.h"],
    deps = [
        "//src/jsmalloc/util:math",
    ],
)

cc_library(
    name = "jsmalloc",
    hdrs = ["jsmalloc.h"],
    srcs = ["jsmalloc.cc"],
    deps = [
        ":chunky_block",
        "//src:heap_interface",
        "//src/jsmalloc/util:math",
        "//src/jsmalloc/collections:intrusive_linked_list",
    ],
    visibility = ["//visibility:public"],
)
