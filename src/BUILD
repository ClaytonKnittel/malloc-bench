load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

bool_flag(
    name = "enable_perfetto",
    build_setting_default = False,
    visibility = ["//visibility:public"],
)

config_setting(
    name = "perfetto_enabled",
    flag_values = {":enable_perfetto": "True"},
)

cc_binary(
    name = "driver",
    srcs = ["driver.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":correctness_checker",
        ":heap_factory",
        ":mmap_heap_factory",
        ":perfetto",
        ":perftest",
        ":tracefile_executor",
        ":tracefile_reader",
        ":utiltest",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "perftest",
    srcs = ["perftest.cc"],
    hdrs = ["perftest.h"],
    deps = [
        ":heap_factory",
        ":malloc_runner",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/time",
    ],
)

cc_library(
    name = "utiltest",
    srcs = ["utiltest.cc"],
    hdrs = ["utiltest.h"],
    deps = [
        ":allocator_interface",
        ":heap_factory",
        ":malloc_runner",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
        "@folly",
    ],
)

cc_library(
    name = "allocator_interface",
    hdrs = ["allocator_interface.h"],
    deps = [
        "//src/pkmalloc",
    ],
)

cc_library(
    name = "correctness_checker",
    srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        ":heap_factory",
        ":malloc_runner",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/algorithm:container",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
        "@folly",
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
        ":mmap_heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "heap_factory",
    srcs = ["heap_factory.cc"],
    hdrs = ["heap_factory.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
        ":util",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@cc-util//util:absl_util",
    ],
)

cc_binary(
    name = "liballoc.so",
    srcs = ["libc_override.cc"],
    copts = ["-fexceptions"],
    linkshared = 1,
    deps = [
        ":allocator_interface",
    ],
)

cc_library(
    name = "mmap_heap_factory",
    srcs = ["mmap_heap_factory.cc"],
    hdrs = ["mmap_heap_factory.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_factory",
        ":heap_interface",
        ":mmap_heap",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "heap_interface",
    srcs = ["heap_interface.cc"],
    hdrs = ["heap_interface.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
    ],
)

cc_library(
    name = "mmap_heap",
    srcs = ["mmap_heap.cc"],
    hdrs = ["mmap_heap.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "sim_heap",
    hdrs = ["sim_heap.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
    ],
)

cc_binary(
    name = "tracefile_parser",
    srcs = ["tracefile_parser.cc"],
    data = [
        "//traces",
    ],
    deps = [
        "//proto:tracefile_cc_proto",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
        "@protobuf",
        "@protobuf//src/google/protobuf/io",
        "@re2",
    ],
)

cc_library(
    name = "tracefile_executor",
    hdrs = ["tracefile_executor.h"],
    deps = [
        ":perfetto",
        ":tracefile_reader",
        "//proto:tracefile_cc_proto",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/time",
        "@cc-util//util:absl_util",
        "@folly",
    ],
)

cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    deps = [
        "//proto:tracefile_cc_proto",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "malloc_runner",
    hdrs = ["malloc_runner.h"],
    deps = [
        ":allocator_interface",
        ":heap_factory",
        ":tracefile_executor",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
)

cc_library(
    name = "perfetto",
    srcs = ["perfetto.cc"],
    hdrs = ["perfetto.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        "@abseil-cpp//absl/flags:flag",
    ] + select({
        ":perfetto_enabled": ["@com_google_perfetto//:libperfetto_client_experimental"],
        "//conditions:default": [],
    }),
)
