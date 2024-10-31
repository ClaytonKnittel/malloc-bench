load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "gflags",
    strip_prefix = "gflags-master",
    urls = ["https://github.com/gflags/gflags/archive/master.zip"],
)

http_archive(
    name = "com_github_google_glog",
    strip_prefix = "glog-master",
    urls = ["https://github.com/google/glog/archive/master.zip"],
)

http_archive(
    name = "com_github_google_double_conversion",
    build_file = "//:double_conversion.BUILD",
    strip_prefix = "double-conversion-master",
    urls = ["https://github.com/google/double-conversion/archive/master.zip"],
)

http_archive(
    name = "folly",
    build_file = "//:folly.BUILD",
    strip_prefix = "folly-main",
    urls = ["https://github.com/facebook/folly/archive/main.zip"],
)
