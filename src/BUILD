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
        ":mmap_heap_factory",
        ":perftest",
        "@abseil-cpp//absl/status:statusor",
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
        ":heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/time",
        "@cc-util//util:absl_util",
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
        ":heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "allocator_interface",
    hdrs = ["allocator_interface.h"],
    deps = [
        ":heap_factory",
        "//src/ckmalloc",
    ],
)

cc_library(
    name = "correctness_checker",
    srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        ":allocator_interface",
        ":heap_factory",
        ":rng",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/algorithm:container",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

cc_test(
    name = "correctness_test",
    srcs = ["correctness_test.cc"],
    data = [
        "//traces",
    ],
    shard_count = 33,
    deps = [
        ":heap_factory",
        ":mmap_heap_factory",
        ":tracefile_executor",
        ":tracefile_reader",
        "//src/ckmalloc:common",
        "//src/ckmalloc:main_allocator_test_fixture",
        "//src/ckmalloc:metadata_manager_test_fixture",
        "//src/ckmalloc:slab_manager_test_fixture",
        "//src/ckmalloc:small_allocator_test_fixture",
        "//src/ckmalloc:testlib",
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
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
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
    name = "tracefile_cleaner",
    srcs = ["tracefile_cleaner.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":tracefile_reader",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "tracefile_executor",
    srcs = ["tracefile_executor.cc"],
    hdrs = ["tracefile_executor.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "rng",
    hdrs = ["rng.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        "@abseil-cpp//absl/status:statusor",
    ],
)
