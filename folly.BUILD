cc_library(
    name = "folly",
    srcs = [
        "folly/synchronization/Hazptr.cpp",
    ],
    hdrs = glob([
        "folly/**/*.h",
    ]),
    defines = [
        "FOLLY_NO_CONFIG",
        "FOLLY_HAVE_MEMRCHR",
        "FOLLY_HAVE_SENDMMSG",
        "FOLLY_HAVE_RECVMMSG",
    ],
    includes = [""],
    visibility = ["//visibility:public"],
    deps = [
        "@boost//:preprocessor",
        "@com_github_google_double_conversion//:double-conversion",
        "@com_github_google_glog//:glog",
        "@fmt",
    ],
)
