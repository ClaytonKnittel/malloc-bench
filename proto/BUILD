load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
  name = "tracefile_proto",
  srcs = ["tracefile.proto"],
)

cc_proto_library(
  name = "tracefile_cc_proto",
  visibility = ["//visibility:public"],
  deps = [
    ":tracefile_proto",
  ],
)
