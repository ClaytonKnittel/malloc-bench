load("//traces:compression.bzl", "gz_inflated_filegroup")

gz_inflated_filegroup(
    name = "traces",
    srcs = glob(["*.trace.gz"]),
    visibility = ["//visibility:public"],
)
