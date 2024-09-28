cc_library(
    name = "small_size_free_list",
    srcs = ["small_size_free_list.cc"],
    hdrs = ["small_size_free_list.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/blocks:free_block",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/collections:rbtree",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:bitset",
        "//src/jsmalloc/util:math",
    ],
)

cc_library(
    name = "rbtree_free_list",
    srcs = ["rbtree_free_list.cc"],
    hdrs = ["rbtree_free_list.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/blocks:free_block",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/collections:rbtree",
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:bitset",
        "//src/jsmalloc/util:math",
    ],
)

cc_library(
    name = "learned_size_free_list",
    srcs = ["learned_size_free_list.cc"],
    hdrs = ["learned_size_free_list.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/blocks:free_block",
        "//src/jsmalloc/collections:intrusive_linked_list",
        "//src/jsmalloc/collections:rbtree",
        "//src/jsmalloc/util:assert",
        "@abseil-cpp//absl/hash",
        "//src/jsmalloc/util:bitset",
        "//src/jsmalloc/util:math",
    ],
)
