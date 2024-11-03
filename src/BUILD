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

cc_binary(
    name = "perf",
    srcs = ["perf.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":perftest",
        ":tracefile_reader",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "perftest",
    srcs = ["perftest.cc"],
    hdrs = ["perftest.h"],
    deps = [
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
        ":heap_factory",
        ":malloc_runner",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
        "@folly",
    ],
)

cc_library(
    name = "allocator_interface",
    hdrs = ["allocator_interface.h"],
    deps = [
        "//src/ckmalloc",
    ],
)

cc_library(
    name = "test_allocator_interface",
    hdrs = ["test_allocator_interface.h"],
    deps = [
        ":heap_factory",
        "//src/ckmalloc",
        "//src/ckmalloc:testlib",
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
    shard_count = 46,
    deps = [
        ":heap_factory",
        ":tracefile_executor",
        ":tracefile_reader",
        "//src/ckmalloc:common",
        "//src/ckmalloc:freelist",
        "//src/ckmalloc:large_allocator_test_fixture",
        "//src/ckmalloc:main_allocator_test_fixture",
        "//src/ckmalloc:metadata_manager_test_fixture",
        "//src/ckmalloc:slab_manager_test_fixture",
        "//src/ckmalloc:small_allocator_test_fixture",
        "//src/ckmalloc:testlib",
        "//src/ckmalloc:util",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
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
    visibility = ["//:__subpackages__"],
    deps = [
        ":tracefile_reader",
        "//proto:tracefile_cc_proto",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@cc-util//util:absl_util",
        "@folly",
    ],
)

cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    visibility = ["//:__subpackages__"],
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
        ":test_allocator_interface",
        ":tracefile_executor",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
)
