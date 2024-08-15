cc_library(
    name = "ckmalloc",
    srcs = ["ckmalloc.cc"],
    hdrs = ["ckmalloc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":state",
    ],
)

cc_library(
    name = "block",
    srcs = ["block.cc"],
    hdrs = ["block.h"],
    deps = [
        ":common",
        ":linked_list",
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
    deps = [
        "//src:singleton_heap",
    ],
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
        ":linked_list",
        ":util",
    ],
)

cc_test(
    name = "freelist_test",
    srcs = ["freelist_test.cc"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":linked_list",
        ":testlib",
        ":util",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
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
    name = "main_allocator",
    hdrs = ["main_allocator.h"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":slab_manager",
        ":slab_map",
        ":util",
    ],
)

cc_test(
    name = "main_allocator_test",
    srcs = ["main_allocator_test.cc"],
    deps = [
        ":block",
        ":linked_list",
        ":main_allocator",
        ":main_allocator_test_fixture",
        ":slab_manager_test_fixture",
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
    deps = [
        ":common",
        ":freelist",
        ":main_allocator",
        ":page_id",
        ":slab",
        ":slab_manager_test_fixture",
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
        ":page_id",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":util",
    ],
)

cc_test(
    name = "metadata_manager_test",
    srcs = ["metadata_manager_test.cc"],
    deps = [
        ":common",
        ":metadata_manager_test_fixture",
        ":slab",
        ":slab_manager_test_fixture",
        ":size_class",
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
    deps = [
        ":metadata_manager",
        ":slab",
        ":slab_manager_test_fixture",
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
        ":util",
        "//src:rng",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@cc-util//util:absl_util",
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
        "//src:heap_interface",
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
        "@cc-util//util:gtest_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "slab_manager_test_fixture",
    srcs = ["slab_manager_test_fixture.cc"],
    hdrs = ["slab_manager_test_fixture.h"],
    deps = [
        ":common",
        ":page_id",
        ":slab",
        ":slab_manager",
        ":testlib",
        ":util",
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
        ":common",
    ],
)

cc_library(
    name = "small_freelist",
    srcs = ["small_freelist.cc"],
    hdrs = ["small_freelist.h"],
    deps = [
        ":common",
        ":slab",
        ":slab_manager",
        ":slice",
        ":util",
    ],
)

cc_library(
    name = "state",
    srcs = ["state.cc"],
    hdrs = ["state.h"],
    deps = [
        ":common",
        ":main_allocator",
        ":metadata_manager",
        ":slab",
        ":slab_manager",
        ":slab_map",
        ":util",
        "//src:heap_interface",
    ],
)

cc_library(
    name = "testlib",
    srcs = ["testlib.cc"],
    hdrs = ["testlib.h"],
    deps = [
        ":block",
        ":common",
        ":freelist",
        ":page_id",
        ":slab",
        ":slab_map",
        ":util",
        "//src:heap_interface",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
)
