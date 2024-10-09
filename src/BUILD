cc_binary(
    name = "driver",
    srcs = ["driver.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":correctness_checker",
<<<<<<< HEAD
        ":heap_factory",
        ":mmap_heap_factory",
=======
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
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

cc_library(
    name = "perftest",
    srcs = ["perftest.cc"],
    hdrs = ["perftest.h"],
    data = [
        "//traces",
    ],
    deps = [
        ":allocator_interface",
<<<<<<< HEAD
        ":heap_factory",
        ":tracefile_reader",
=======
        ":singleton_heap",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:flat_hash_map",
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
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
<<<<<<< HEAD
        ":heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
    ],
    data = [
        "//traces",
    ],
)

cc_binary(
    name = "perftest",
    srcs = ["perftest.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":allocator_interface",
        ":tracefile_reader",
        ":util",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/container:flat_hash_map",
    ],
=======
        ":singleton_heap",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
    ],
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
)

cc_library(
    name = "allocator_interface",
<<<<<<< HEAD
    srcs = ["allocator_interface.cc"],
    hdrs = ["allocator_interface.h"],
    deps = [
        ":heap_factory",
        ":heap_interface",
        ":mmap_heap",
=======
    hdrs = ["allocator_interface.h"],
    deps = [
        "//src/pkmalloc:allocated_block",
        "//src/pkmalloc:free_block",
        "//src/pkmalloc:free_list",
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
    ],
)

cc_library(
    name = "correctness_checker",
    srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        ":allocator_interface",
<<<<<<< HEAD
        ":heap_factory",
        ":rng",
        ":tracefile_executor",
        ":tracefile_reader",
        "@abseil-cpp//absl/algorithm:container",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
=======
        ":rng",
        ":singleton_heap",
        ":tracefile_reader",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
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
    deps = [
        ":correctness_checker",
<<<<<<< HEAD
        ":mmap_heap_factory",
        ":tracefile_reader",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
=======
        "@abseil-cpp//absl/status",
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
<<<<<<< HEAD
    name = "heap_factory",
    srcs = ["heap_factory.cc"],
    hdrs = ["heap_factory.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "libc_override",
    srcs = ["libc_override.cc"],
    deps = [
        ":allocator_interface",
    ],
)

cc_shared_library(
    name = "liballoc",
    shared_lib_name = "liballoc.so",
    deps = [
        ":libc_override",
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
=======
    name = "singleton_heap",
    srcs = ["singleton_heap.cc"],
    hdrs = ["singleton_heap.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
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
<<<<<<< HEAD
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
=======
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
    name = "sim_heap",
    hdrs = ["sim_heap.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":heap_interface",
    ],
)

<<<<<<< HEAD
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
    ],
)

cc_library(
    name = "tracefile_executor",
    srcs = ["tracefile_executor.cc"],
    hdrs = ["tracefile_executor.h"],
    deps = [
        ":heap_factory",
        ":tracefile_reader",
        "//proto:tracefile_cc_proto",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

=======
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    deps = [
<<<<<<< HEAD
        "//proto:tracefile_cc_proto",
=======
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "rng",
    hdrs = ["rng.h"],
    deps = [
        "@abseil-cpp//absl/status:statusor",
    ],
)
