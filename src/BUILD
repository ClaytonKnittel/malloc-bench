cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        ":correctness_checker",
        ":tracefile_reader",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "allocator_interface",
    hdrs = ["allocator_interface.h"],
)

cc_library(
    name = "correctness_checker",
    srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        ":allocator_interface",
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

cc_library(
    name = "tracefile_reader",
    srcs = ["tracefile_reader.cc"],
    hdrs = ["tracefile_reader.h"],
    deps = [
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
    ],
)
