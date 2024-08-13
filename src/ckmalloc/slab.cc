#include "src/ckmalloc/slab.h"

#include <ostream>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
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
SmallSlab* Slab::Init(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kSmall;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .small = {},
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
