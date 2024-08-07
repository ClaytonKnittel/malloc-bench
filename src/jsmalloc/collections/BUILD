
cc_library(
    name = "intrusive_linked_list",
    hdrs = ["intrusive_linked_list.h"],
    deps = [
    ],
)

cc_test(
    name = "intrusive_linked_list_test",
    srcs = ["intrusive_linked_list_test.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":intrusive_linked_list",
        "@cc-util//util:gtest_util",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
