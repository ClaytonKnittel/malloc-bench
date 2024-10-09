#pragma once

#include <cstddef>
#include <cstdint>
<<<<<<< HEAD
#include <new>
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/rng.h"
#include "src/tracefile_executor.h"
=======
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"

#include "src/rng.h"
#include "src/singleton_heap.h"
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
#include "src/tracefile_reader.h"

namespace bench {

<<<<<<< HEAD
class CorrectnessChecker : private TracefileExecutor {
 public:
  static constexpr char kFailedTestPrefix[] = "[Failed]";

  static bool IsFailedTestStatus(const absl::Status& status);

  static absl::Status Check(TracefileReader& reader, HeapFactory& heap_factory,
                            bool verbose = false);
=======
class CorrectnessChecker {
 public:
  static constexpr const char* kFailedTestPrefix = "[Failed]";

  static bool IsFailedTestStatus(const absl::Status& status);

  static absl::Status Check(const std::string& tracefile, bool verbose = false);
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using Map = absl::btree_map<void*, AllocatedBlock>;
<<<<<<< HEAD

  CorrectnessChecker(TracefileReader&& reader, HeapFactory& heap_factory);

  void InitializeHeap(HeapFactory& heap_factory) override;
  absl::StatusOr<void*> Malloc(size_t size) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr) override;

  absl::StatusOr<void*> Alloc(size_t nmemb, size_t size, bool is_calloc);

  absl::Status HandleNewAllocation(void* ptr, size_t size, bool is_calloc);
=======
  using IdMap = absl::flat_hash_map<void*, void*>;

  explicit CorrectnessChecker(TracefileReader&& reader);

  absl::Status Run();
  absl::Status ProcessTracefile();

  absl::Status Malloc(size_t nmemb, size_t size, void* id, bool is_calloc);
  absl::Status Realloc(void* orig_id, size_t size, void* new_id);
  absl::Status Free(void* id);

  absl::Status HandleNewAllocation(void* id, void* ptr, size_t size,
                                   bool is_calloc);
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

  // Validates that a new block doesn't overlap with any existing block, and
  // that it satisfies alignment requirements.
  absl::Status ValidateNewBlock(void* ptr, size_t size) const;

  static void FillMagicBytes(void* ptr, size_t size, uint64_t magic_bytes);
  // Checks if pointer is filled with magic_bytes, and if any byte differs,
  // returns the offset from the beginning of the block of the first differing
  // byte.
  static absl::Status CheckMagicBytes(void* ptr, size_t size,
                                      uint64_t magic_bytes);

<<<<<<< HEAD
  std::optional<typename Map::const_iterator> FindContainingBlock(
      void* ptr) const;

  HeapFactory* const heap_factory_;

  Map allocated_blocks_;
=======
  std::optional<Map::const_iterator> FindContainingBlock(void* ptr) const;

  TracefileReader reader_;

  SingletonHeap* heap_;

  Map allocated_blocks_;
  // A map from the pointer value ID from the trace to the actual pointer value
  // allocated by the allocator.
  IdMap id_map_;
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

  util::Rng rng_;

  bool verbose_;
};

}  // namespace bench
