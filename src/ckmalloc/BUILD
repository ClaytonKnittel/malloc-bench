cc_library(
    name = "ckmalloc",
    srcs = ["ckmalloc.cc"],
    hdrs = ["ckmalloc.h"],
    visibility = ["//visibility:public"],
    deps = [
    ],
)

cc_library(
    name = "fake_allocator",
    srcs = ["fake_allocator.cc"],
    hdrs = ["fake_allocator.h"],
    deps = [
        ":util",
        "//src:fake_heap",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
    ],
)
