cc_library(
    name = "intrusive_linked_list",
    hdrs = ["intrusive_linked_list.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "intrusive_linked_list_test",
    srcs = ["intrusive_linked_list_test.cc"],
    deps = [
        ":intrusive_linked_list",
        "//src/jsmalloc/util:twiddle",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
cc_library(
    name = "intrusive_stack",
    hdrs = ["intrusive_stack.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:twiddle",
    ],
)

cc_test(
    name = "intrusive_stack_test",
    srcs = ["intrusive_stack_test.cc"],
    deps = [
        ":intrusive_stack",
        "//src/jsmalloc/util:twiddle",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "rbtree",
    hdrs = ["rbtree.h"],
    srcs = ["rbtree.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//src/jsmalloc/util:assert",
        "//src/jsmalloc/util:twiddle",
    ],
)
