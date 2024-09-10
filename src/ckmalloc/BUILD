cc_library(
    name = "ckmalloc",
    srcs = ["ckmalloc.cc"],
    hdrs = ["ckmalloc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":common",
        ":local_cache",
        ":main_allocator",
        ":global_state",
        "//src:heap_factory",
    ],
)

cc_library(
    name = "block",
    srcs = ["block.cc"],
    hdrs = ["block.h"],
    deps = [
        ":common",
        ":linked_list",
        ":red_black_tree",
        ":util",
    ],
)

cc_test(
    name = "block_test",
    srcs = ["block_test.cc"],
    deps = [
        ":block",
        ":common",
        ":util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "common",
    hdrs = ["common.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "free_slab",
    hdrs = ["free_slab.h"],
    deps = [
        ":linked_list",
        ":red_black_tree",
    ],
)

cc_library(
    name = "freelist",
    srcs = ["freelist.cc"],
    hdrs = ["freelist.h"],
    deps = [
        ":block",
        ":common",
        ":linked_list",
        ":red_black_tree",
        ":util",
        "@cc-util//util:bit_set",
    ],
)

cc_library(
    name = "heap_printer",
    srcs = ["heap_printer.cc"],
    hdrs = ["heap_printer.h"],
    deps = [
        ":block",
        ":common",
        ":metadata_manager",
        ":page_id",
        ":size_class",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":slice_id",
        ":util",
        "//src:heap_interface",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:print_colors",
    ],
)

cc_library(
    name = "large_allocator",
    hdrs = ["large_allocator.h"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":util",
    ],
)

cc_test(
    name = "large_allocator_test",
    srcs = ["large_allocator_test.cc"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":large_allocator_test_fixture",
        ":page_id",
        ":slab",
        ":slab_manager",
        ":slab_manager_test_fixture",
        ":testlib",
        ":util",
        "//src:heap_interface",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "large_allocator_test_fixture",
    srcs = ["large_allocator_test_fixture.cc"],
    hdrs = ["large_allocator_test_fixture.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":large_allocator",
        ":page_id",
        ":slab",
        ":slab_manager_test_fixture",
        ":testlib",
        ":util",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "linked_list",
    srcs = ["linked_list.cc"],
    hdrs = ["linked_list.h"],
    deps = [
        ":util",
    ],
)

cc_test(
    name = "linked_list_test",
    srcs = ["linked_list_test.cc"],
    deps = [
        ":linked_list",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "local_cache",
    srcs = ["local_cache.cc"],
    hdrs = ["local_cache.h"],
    deps = [
        ":block",
        ":common",
        ":util",
    ],
)

cc_library(
    name = "main_allocator",
    hdrs = ["main_allocator.h"],
    deps = [
        ":common",
        ":large_allocator",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":small_allocator",
        ":util",
    ],
)

cc_test(
    name = "main_allocator_test",
    srcs = ["main_allocator_test.cc"],
    deps = [
        ":block",
        ":common",
        ":large_allocator_test_fixture",
        ":main_allocator",
        ":main_allocator_test_fixture",
        ":slab_manager_test_fixture",
        ":small_allocator_test_fixture",
        ":testlib",
        "//src:heap_interface",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "main_allocator_test_fixture",
    srcs = ["main_allocator_test_fixture.cc"],
    hdrs = ["main_allocator_test_fixture.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":common",
        ":freelist",
        ":large_allocator_test_fixture",
        ":local_cache",
        ":main_allocator",
        ":slab",
        ":slab_manager_test_fixture",
        ":small_allocator_test_fixture",
        ":testlib",
        ":util",
        "//src:rng",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "metadata_manager",
    hdrs = ["metadata_manager.h"],
    deps = [
        ":common",
        ":slab",
        ":slab_map",
        ":util",
        "//src:heap_factory",
        "//src:heap_interface",
    ],
)

cc_test(
    name = "metadata_manager_test",
    srcs = ["metadata_manager_test.cc"],
    deps = [
        ":common",
        ":metadata_manager_test_fixture",
        ":slab",
        ":testlib",
        ":util",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "metadata_manager_test_fixture",
    srcs = ["metadata_manager_test_fixture.cc"],
    hdrs = ["metadata_manager_test_fixture.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_manager",
        ":slab",
        ":testlib",
        ":util",
        "//src:rng",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "page_id",
    hdrs = ["page_id.h"],
    deps = [
        ":common",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "red_black_tree",
    srcs = ["red_black_tree.cc"],
    hdrs = ["red_black_tree.h"],
    deps = [
        ":util",
    ],
)

cc_test(
    name = "red_black_tree_test",
    srcs = ["red_black_tree_test.cc"],
    deps = [
        ":red_black_tree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "size_class",
    hdrs = ["size_class.h"],
    deps = [
        ":common",
        ":util",
    ],
)

cc_library(
    name = "slab",
    srcs = ["slab.cc"],
    hdrs = ["slab.h"],
    deps = [
        ":block",
        ":common",
        ":page_id",
        ":size_class",
        ":slice",
        ":slice_id",
        ":util",
    ],
)

cc_test(
    name = "slab_test",
    srcs = ["slab_test.cc"],
    deps = [
        ":common",
        ":page_id",
        ":size_class",
        ":slab",
        ":slice",
        ":slice_id",
        ":util",
        "//src:rng",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "slab_manager",
    hdrs = ["slab_manager.h"],
    deps = [
        ":block",
        ":common",
        ":free_slab",
        ":linked_list",
        ":page_id",
        ":slab",
        ":slab_map",
        ":util",
        "//src:heap_factory",
        "@cc-util//util:std_util",
    ],
)

cc_test(
    name = "slab_manager_test",
    srcs = ["slab_manager_test.cc"],
    deps = [
        ":common",
        ":page_id",
        ":slab",
        ":slab_manager_test_fixture",
        ":testlib",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "slab_manager_test_fixture",
    srcs = ["slab_manager_test_fixture.cc"],
    hdrs = ["slab_manager_test_fixture.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":common",
        ":page_id",
        ":slab",
        ":slab_manager",
        ":testlib",
        ":util",
        "//src:heap_interface",
        "//src:rng",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
    ],
)

cc_library(
    name = "slab_map",
    hdrs = ["slab_map.h"],
    deps = [
        ":common",
        ":page_id",
        ":util",
        "@cc-util//util:std_util",
    ],
)

cc_test(
    name = "slab_map_test",
    srcs = ["slab_map_test.cc"],
    deps = [
        ":page_id",
        ":slab",
        ":slab_map",
        ":testlib",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "slice",
    hdrs = ["slice.h"],
    deps = [
        ":slice_id",
    ],
)

cc_library(
    name = "slice_id",
    hdrs = ["slice_id.h"],
    deps = [
        ":common",
        ":util",
    ],
)

cc_library(
    name = "small_allocator",
    hdrs = ["small_allocator.h"],
    deps = [
        ":common",
        ":page_id",
        ":size_class",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":slice",
        ":util",
    ],
)

cc_test(
    name = "small_allocator_test",
    srcs = ["small_allocator_test.cc"],
    deps = [
        ":common",
        ":size_class",
        ":slab",
        ":slab_manager_test_fixture",
        ":slice",
        ":small_allocator_test_fixture",
        ":testlib",
        "//src:heap_interface",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "small_allocator_test_fixture",
    srcs = ["small_allocator_test_fixture.cc"],
    hdrs = ["small_allocator_test_fixture.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":page_id",
        ":size_class",
        ":slab",
        ":slab_manager_test_fixture",
        ":small_allocator",
        ":testlib",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "global_state",
    srcs = ["global_state.cc"],
    hdrs = ["global_state.h"],
    deps = [
        ":main_allocator",
        ":metadata_manager",
        ":slab_manager",
        ":slab_map",
        ":small_allocator",
        "//src:heap_factory",
    ],
)

cc_library(
    name = "testlib",
    srcs = ["testlib.cc"],
    hdrs = ["testlib.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":block",
        ":common",
        ":page_id",
        ":slab",
        ":slab_map",
        ":util",
        "//src:heap_factory",
        "//src:heap_interface",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_binary(
    name = "trace_replayer",
    srcs = ["trace_replayer.cc"],
    data = [
        "//traces",
    ],
    deps = [
        ":ckmalloc",
        ":heap_printer",
        ":global_state",
        "//src:heap_factory",
        "//src:mmap_heap_factory",
        "//src:tracefile_executor",
        "//src:tracefile_reader",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:absl_util",
        "@cc-util//util:csi",
        "@cc-util//util:print_colors",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
)
