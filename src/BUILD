cc_binary(
    name = "main",
    srcs = ["main.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":correctness_checker",
        ":perftest",
        ":tracefile_reader",
        ":util",
        ":utiltest",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "perftest",
    srcs = ["perftest.cc"],
    hdrs = ["perftest.h"],
    data = [
        "//traces",
    ],
    deps = [
        ":allocator_interface",
        ":tracefile_reader",
        ":util",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/time",
    ],
)

cc_library(
    name = "utiltest",
    srcs = ["utiltest.cc"],
    hdrs = ["utiltest.h"],
    data = [
        "//traces",
    ],
    deps = [
        ":allocator_interface",
        ":fake_heap",
        ":tracefile_reader",
        ":util",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "allocator_interface",
    hdrs = ["allocator_interface.h"],
    deps = [
        ":fake_heap",
    ],
)

cc_library(
    name = "correctness_checker",
    srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        ":allocator_interface",
        ":fake_heap",
        ":rng",
        ":tracefile_reader",
        ":util",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "correctness_test",
    srcs = ["correctness_test.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":correctness_checker",
        ":util",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_heap",
    srcs = ["fake_heap.cc"],
    hdrs = ["fake_heap.h"],
    deps = [
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    deps = [
        ":util",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "rng",
    hdrs = ["rng.h"],
    deps = [
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
    ],
)
