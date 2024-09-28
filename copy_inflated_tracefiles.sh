#!/usr/bin/sh

set -e

bazel build --show_result=0 --noshow_progress --noshow_loading_progress //traces
cp bazel-bin/traces/*.trace traces/
find traces/ -name "*.trace" -exec chmod 644 {} \;
