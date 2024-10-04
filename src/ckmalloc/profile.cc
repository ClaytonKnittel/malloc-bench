#include <cstdint>
#include <filesystem>
#include <iostream>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"

#include "src/ckmalloc/common.h"
#include "src/tracefile_reader.h"

using bench::TracefileReader;
using bench::proto::TraceLine;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;

int main() {
  absl::flat_hash_map<uint64_t, uint64_t> sizes;

  uint64_t smalls = 0;
  uint64_t larges = 0;
  uint64_t mmaps = 0;
  for (const directory_entry& dir_entry : directory_iterator("traces/")) {
    if (dir_entry.path().extension() == ".trace") {
      auto result = TracefileReader::Open(dir_entry.path());
      if (!result.ok()) {
        std::cerr << "Failed to open " << dir_entry.path() << ": "
                  << result.status() << std::endl;
        continue;
      }

      TracefileReader reader = std::move(result.value());
      for (const TraceLine& line : reader) {
        if (line.op_case() == TraceLine::kMalloc) {
          size_t size =
              line.malloc().input_size() <= 8
                  ? 8
                  : ((line.malloc().input_size() + 0xf) & ~UINT64_C(0xf));
          ++sizes[size];

          if (ckmalloc::IsSmallSize(line.malloc().input_size())) {
            smalls++;
          } else if (ckmalloc::IsMmapSize(line.malloc().input_size())) {
            mmaps++;
          } else {
            larges++;
          }
        }
      }
    }
  }

  std::cout << "smalls: " << smalls << std::endl;
  std::cout << "larges: " << larges << std::endl;
  std::cout << "mmaps:  " << mmaps << std::endl;

  std::vector<std::pair<uint64_t, uint64_t>> szs(sizes.begin(), sizes.end());
  absl::c_sort(
      szs, [](const auto& a, const auto& b) { return a.second > b.second; });

  for (uint64_t i = 0; i < 10; i++) {
    std::cout << szs[i].first << " : " << szs[i].second << std::endl;
  }

  return 0;
}