
cc_library(
    name = "intrusive_linked_list",
    hdrs = ["intrusive_linked_list.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "intrusive_linked_list_test",
    srcs = ["intrusive_linked_list_test.cc"],
    deps = [
        ":intrusive_linked_list",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
