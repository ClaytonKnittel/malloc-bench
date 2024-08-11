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
        ":linked_list",
        ":util",
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
    hdrs = ["freelist.h"],
    deps = [
        ":block",
        ":common",
        ":slab_manager",
        ":slab_map",
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
    name = "slab",
    srcs = ["slab.cc"],
    hdrs = ["slab.h"],
    deps = [
        ":page_id",
        ":util",
    ],
)

cc_library(
    name = "slab_manager",
    hdrs = ["slab_manager.h"],
    deps = [
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
        ":slab",
        ":util",
        "@cc-util//util:std_util",
    ],
)

cc_test(
    name = "slab_map_test",
    srcs = ["slab_map_test.cc"],
    deps = [
        ":page_id",
        ":slab_map",
        ":testlib",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "state",
    srcs = ["state.cc"],
    hdrs = ["state.h"],
    deps = [
        ":common",
        ":freelist",
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
        ":common",
        ":page_id",
        ":slab",
        ":slab_map",
        "//src:heap_interface",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
    ],
)
