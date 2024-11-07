load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_github_google_double_conversion",
    build_file = "//:double_conversion.BUILD",
    integrity = "sha256-QIABQjX5CFT/rebRxCOUCzFLvKJzozgjXwSdopbkcYM=",
    strip_prefix = "double-conversion-3.3.0",
    urls = ["https://github.com/google/double-conversion/archive/v3.3.0.zip"],
)

http_archive(
    name = "folly",
    build_file = "//:folly.BUILD",
    integrity = "sha256-+ZknvwBXT9r0PuOzZPZkDY1gybY2PutpEenORYX889Y=",
    strip_prefix = "folly-2024.10.28.00",
    urls = ["https://github.com/facebook/folly/archive/v2024.10.28.00.zip"],
)

# For use with perfetto
new_local_repository(
    name = "perfetto_cfg",
    build_file_content = "",
    path = "build/perfetto_overrides",
)

http_archive(
    name = "com_google_perfetto",
    integrity = "sha256-6Nw4/KHm7GqVOA5Nj2VM6PsjpGeIqXDwCMoIz7UIqzY=",
    repo_mapping = {
        "@perfetto": "@com_google_perfetto",
        "@perfetto_dep_zlib": "@zlib",
        "@com_google_protobuf": "@protobuf",
    },
    strip_prefix = "perfetto-48.1",
    urls = ["https://github.com/google/perfetto/archive/v48.1.zip"],
)
