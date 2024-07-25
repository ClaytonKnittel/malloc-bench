cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
      ":tracefile_reader",
    ],
)

cc_library(
    name = "alloc_interface",
    hdrs = ["alloc_interface.h"],
)

cc_library(
    name = "correctness_checker",
    # srcs = ["correctness_checker.cc"],
    hdrs = ["correctness_checker.h"],
    deps = [
        "@abseil-cpp//absl/status",
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
