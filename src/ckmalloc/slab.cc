#include "src/ckmalloc/slab.h"

#include <ostream>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

std::ostream& operator<<(std::ostream& ostr, SlabType slab_type) {
  switch (slab_type) {
    case SlabType::kUnmapped: {
      return ostr << "kUnmapped";
    }
    case SlabType::kFree: {
      return ostr << "kFree";
    }
    case SlabType::kSmall: {
      return ostr << "kSmall";
    }
    case SlabType::kLarge: {
      return ostr << "kLarge";
    }
  }
}

SmallSlabMetadata::SmallSlabMetadata(class SizeClass size_class)
    : size_class_(size_class),
      freelist_node_offset_(FreelistNodesPerSlice(size_class) - 1),
      uninitialized_count_(size_class.MaxSlicesPerSlab()) {}

bool SmallSlabMetadata::Empty() const {
  return allocated_count_ == 0;
}

bool SmallSlabMetadata::Full() const {
  return freelist_ == SliceId::Nil() && uninitialized_count_ == 0;
}

AllocatedSlice* SmallSlabMetadata::PopSlice(void* slab_start) {
  SliceId id = SliceId::Nil();
  if (freelist_ != SliceId::Nil()) {
    FreeSlice* slice = SliceFromId(slab_start, freelist_);
    SliceId next_in_list = slice->IdAt(freelist_node_offset_);

    if (freelist_node_offset_ == 0) {
      id = freelist_;
      freelist_ = next_in_list;
      freelist_node_offset_ = FreelistNodesPerSlice(size_class_) - 1;
    } else {
      id = next_in_list;
      freelist_node_offset_--;
    }
  } else {
    // If the freelist is empty, we can allocate more slices from the end of the
    // allocated space within the slab.
    CK_ASSERT_GT(uninitialized_count_, 0);

    id = SliceId((size_class_.MaxSlicesPerSlab() - uninitialized_count_) *
                 size_class_.SliceSize());
    uninitialized_count_--;
  }

  CK_ASSERT_NE(id, SliceId::Nil());
  allocated_count_++;
  return SliceFromId(slab_start, id)->ToAllocated();
}

void SmallSlabMetadata::PushSlice(void* slab_start, FreeSlice* slice) {
  SliceId slice_id = SliceIdForSlice(slab_start, slice);

  if (freelist_node_offset_ == FreelistNodesPerSlice(size_class_) - 1) {
    slice->SetId(0, freelist_);
    freelist_ = slice_id;
    freelist_node_offset_ = 0;
  } else {
    CK_ASSERT_NE(freelist_, SliceId::Nil());
    FreeSlice* head = SliceFromId(slab_start, freelist_);
    freelist_node_offset_++;
    head->SetId(freelist_node_offset_, slice_id);
  }

  allocated_count_--;
}

template <>
UnmappedSlab* Slab::Init(UnmappedSlab* next) {
  type_ = SlabType::kUnmapped;
  unmapped = {
    .next_ = next,
  };

  return static_cast<UnmappedSlab*>(this);
}

template <>
FreeSlab* Slab::Init(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kFree;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .free = {},
  };

  return static_cast<FreeSlab*>(this);
}

template <>
SmallSlab* Slab::Init(PageId start_id, uint32_t n_pages, SizeClass size_class) {
  type_ = SlabType::kSmall;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .small_meta_ = SmallSlabMetadata(size_class),
  };

  return static_cast<SmallSlab*>(this);
}

template <>
LargeSlab* Slab::Init(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kLarge;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .large = {
      .allocated_bytes_ = 0,
    },
  };

  LargeSlab* slab = static_cast<LargeSlab*>(this);
  return slab;
}

UnmappedSlab* Slab::ToUnmapped() {
  CK_ASSERT_EQ(Type(), SlabType::kUnmapped);
  return static_cast<UnmappedSlab*>(this);
}

const UnmappedSlab* Slab::ToUnmapped() const {
  CK_ASSERT_EQ(Type(), SlabType::kUnmapped);
  return static_cast<const UnmappedSlab*>(this);
}

MappedSlab* Slab::ToMapped() {
  CK_ASSERT_NE(Type(), SlabType::kUnmapped);
  return static_cast<MappedSlab*>(this);
}

const MappedSlab* Slab::ToMapped() const {
  CK_ASSERT_NE(Type(), SlabType::kUnmapped);
  return static_cast<const MappedSlab*>(this);
}

FreeSlab* Slab::ToFree() {
  CK_ASSERT_EQ(Type(), SlabType::kFree);
  return static_cast<FreeSlab*>(this);
}

const FreeSlab* Slab::ToFree() const {
  CK_ASSERT_EQ(Type(), SlabType::kFree);
  return static_cast<const FreeSlab*>(this);
}

AllocatedSlab* Slab::ToAllocated() {
  CK_ASSERT_NE(Type(), SlabType::kUnmapped);
  CK_ASSERT_NE(Type(), SlabType::kFree);
  return static_cast<AllocatedSlab*>(this);
}

const AllocatedSlab* Slab::ToAllocated() const {
  CK_ASSERT_NE(Type(), SlabType::kUnmapped);
  CK_ASSERT_NE(Type(), SlabType::kFree);
  return static_cast<const AllocatedSlab*>(this);
}

SmallSlab* Slab::ToSmall() {
  CK_ASSERT_EQ(Type(), SlabType::kSmall);
  return static_cast<SmallSlab*>(this);
}

const SmallSlab* Slab::ToSmall() const {
  CK_ASSERT_EQ(Type(), SlabType::kSmall);
  return static_cast<const SmallSlab*>(this);
}

LargeSlab* Slab::ToLarge() {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  return static_cast<LargeSlab*>(this);
}

const LargeSlab* Slab::ToLarge() const {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  return static_cast<const LargeSlab*>(this);
}

UnmappedSlab* UnmappedSlab::NextUnmappedSlab() {
  CK_ASSERT_EQ(type_, SlabType::kUnmapped);
  return unmapped.next_;
}

const UnmappedSlab* UnmappedSlab::NextUnmappedSlab() const {
  CK_ASSERT_EQ(type_, SlabType::kUnmapped);
  return unmapped.next_;
}

void UnmappedSlab::SetNextUnmappedSlab(UnmappedSlab* next) {
  CK_ASSERT_EQ(type_, SlabType::kUnmapped);
  unmapped.next_ = next;
}

PageId MappedSlab::StartId() const {
  CK_ASSERT_NE(type_, SlabType::kUnmapped);
  return mapped.id_;
}

PageId MappedSlab::EndId() const {
  CK_ASSERT_NE(type_, SlabType::kUnmapped);
  return mapped.id_ + Pages() - 1;
}

uint32_t MappedSlab::Pages() const {
  CK_ASSERT_NE(type_, SlabType::kUnmapped);
  return mapped.n_pages_;
}

SmallSlabMetadata& SmallSlab::Metadata() {
  CK_ASSERT_EQ(type_, SlabType::kSmall);
  return mapped.small_meta_;
}

/* static */
uint32_t LargeSlab::NPagesForBlock(size_t user_size) {
  return CeilDiv<uint32_t>(Block::BlockSizeForUserSize(user_size) +
                               Block::kFirstBlockInSlabOffset +
                               Block::kMetadataOverhead,
                           kPageSize);
}

uint64_t LargeSlab::MaxBlockSize() const {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  return AlignDown(mapped.n_pages_ * kPageSize -
                       Block::kFirstBlockInSlabOffset -
                       Block::kMetadataOverhead,
                   kDefaultAlignment);
}

void LargeSlab::AddAllocation(uint64_t n_bytes) {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  mapped.large.allocated_bytes_ += n_bytes;
}

void LargeSlab::RemoveAllocation(uint64_t n_bytes) {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  mapped.large.allocated_bytes_ -= n_bytes;
}

uint64_t LargeSlab::AllocatedBytes() const {
  CK_ASSERT_EQ(Type(), SlabType::kLarge);
  return mapped.large.allocated_bytes_;
}

}  // namespace ckmalloc
