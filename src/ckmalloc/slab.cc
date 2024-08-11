#include "src/ckmalloc/slab.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

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
MetadataSlab* Slab::Init(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kMetadata;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .metadata = {},
  };

  return static_cast<MetadataSlab*>(this);
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
    .large = {},
  };

  return static_cast<LargeSlab*>(this);
}

UnmappedSlab* Slab::ToUnmapped() {
  CK_ASSERT(Type() == SlabType::kUnmapped);
  return static_cast<UnmappedSlab*>(this);
}

const UnmappedSlab* Slab::ToUnmapped() const {
  CK_ASSERT(Type() == SlabType::kUnmapped);
  return static_cast<const UnmappedSlab*>(this);
}

MappedSlab* Slab::ToMapped() {
  CK_ASSERT(Type() != SlabType::kUnmapped);
  return static_cast<MappedSlab*>(this);
}

const MappedSlab* Slab::ToMapped() const {
  CK_ASSERT(Type() != SlabType::kUnmapped);
  return static_cast<const MappedSlab*>(this);
}

FreeSlab* Slab::ToFree() {
  CK_ASSERT(Type() == SlabType::kFree);
  return static_cast<FreeSlab*>(this);
}

const FreeSlab* Slab::ToFree() const {
  CK_ASSERT(Type() == SlabType::kFree);
  return static_cast<const FreeSlab*>(this);
}

AllocatedSlab* Slab::ToAllocated() {
  CK_ASSERT(Type() == SlabType::kMetadata || Type() == SlabType::kSmall ||
            Type() == SlabType::kLarge);
  return static_cast<MetadataSlab*>(this);
}

const AllocatedSlab* Slab::ToAllocated() const {
  CK_ASSERT(Type() == SlabType::kMetadata || Type() == SlabType::kSmall ||
            Type() == SlabType::kLarge);
  return static_cast<const MetadataSlab*>(this);
}

MetadataSlab* Slab::ToMetadata() {
  CK_ASSERT(Type() == SlabType::kMetadata);
  return static_cast<MetadataSlab*>(this);
}

const MetadataSlab* Slab::ToMetadata() const {
  CK_ASSERT(Type() == SlabType::kMetadata);
  return static_cast<const MetadataSlab*>(this);
}

SmallSlab* Slab::ToSmall() {
  CK_ASSERT(Type() == SlabType::kSmall);
  return static_cast<SmallSlab*>(this);
}

const SmallSlab* Slab::ToSmall() const {
  CK_ASSERT(Type() == SlabType::kSmall);
  return static_cast<const SmallSlab*>(this);
}

LargeSlab* Slab::ToLarge() {
  CK_ASSERT(Type() == SlabType::kLarge);
  return static_cast<LargeSlab*>(this);
}

const LargeSlab* Slab::ToLarge() const {
  CK_ASSERT(Type() == SlabType::kLarge);
  return static_cast<const LargeSlab*>(this);
}

UnmappedSlab* UnmappedSlab::NextUnmappedSlab() {
  CK_ASSERT(type_ == SlabType::kUnmapped);
  return unmapped.next_;
}

const UnmappedSlab* UnmappedSlab::NextUnmappedSlab() const {
  CK_ASSERT(type_ == SlabType::kUnmapped);
  return unmapped.next_;
}

void UnmappedSlab::SetNextUnmappedSlab(UnmappedSlab* next) {
  CK_ASSERT(type_ == SlabType::kUnmapped);
  unmapped.next_ = next;
}

PageId MappedSlab::StartId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_;
}

PageId MappedSlab::EndId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_ + Pages() - 1;
}

uint32_t MappedSlab::Pages() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.n_pages_;
}

}  // namespace ckmalloc
