#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "src/jsmalloc/util/allocable.h"

namespace jsmalloc {
namespace internal {

template <typename B>
concept BitSetT =
    AllocableT<B, size_t> &&
    requires(B bitset, size_t pos, bool value, void* ptr, size_t num_bits) {
      { bitset.set(pos, value) } -> std::same_as<void>;
      { bitset.set(pos) } -> std::same_as<void>;
      { bitset.test(pos) } -> std::same_as<bool>;
      { bitset.countr_one() } -> std::same_as<size_t>;
      { B::kMaxBits } -> std::convertible_to<size_t>;
    };

template <typename T>
concept PrimitiveBitsT =
    (std::same_as<T, uint8_t> || std::same_as<T, uint16_t> ||
     std::same_as<T, uint32_t> || std::same_as<T, uint64_t>);

/**
 * A bitset with a primitive backing.
 */
template <PrimitiveBitsT T = uint64_t>
class PrimitiveBitSet : public DefaultAllocable<PrimitiveBitSet<T>, size_t> {
 public:
  static constexpr size_t kMaxBits = sizeof(T) * 8;

 private:
  static constexpr auto kOne = static_cast<T>(1);

 public:
  explicit PrimitiveBitSet(size_t /*num_bits*/) {}

  void set(size_t pos, bool value = true) {
    bits_ &= ~(kOne << pos);
    bits_ |= static_cast<T>(value) << pos;
  }

  bool test(size_t pos) const {
    return static_cast<bool>(bits_ & (kOne << pos));
  }

  size_t popcount() const {
    return std::popcount(bits_);
  }

  size_t countr_one() const {
    return std::countr_one(bits_);
  }

 private:
  T bits_ = 0;
};

static_assert(BitSetT<PrimitiveBitSet<uint64_t>>);

/**
 * A multi-level bitset.
 *
 * Allows composing BitSetT classes to an arbitrary depth,
 * while supporting logarithmic countr_one.
 */
template <PrimitiveBitsT FirstLevelPrim, BitSetT SecondLevel>
class MultiLevelBitSet {
 private:
  using This = MultiLevelBitSet<FirstLevelPrim, SecondLevel>;
  using FirstLevel = PrimitiveBitSet<FirstLevelPrim>;
  static constexpr size_t kMaxSecondLevelSize =
      SecondLevel::RequiredSize(SecondLevel::kMaxBits);

 public:
  /** The maximum number of bits this structure can hold. */
  static constexpr size_t kMaxBits =
      FirstLevel::kMaxBits * SecondLevel::kMaxBits;

  static constexpr size_t RequiredSize(size_t num_bits) {
    size_t size = offsetof(This, second_level_);

    size_t second_level_length =
        (num_bits + SecondLevel::kMaxBits - 1) / SecondLevel::kMaxBits;

    // Non-terminal second level blocks are completely packed.
    size += (second_level_length - 1) * kMaxSecondLevelSize;

    // Size of the last block in the second level.
    // This block may be incomplete and we can save some space here.
    size_t remaining_bits =
        num_bits - (second_level_length - 1) * SecondLevel::kMaxBits;
    size += SecondLevel::RequiredSize(remaining_bits);

    return size;
  }

  static MultiLevelBitSet<FirstLevelPrim, SecondLevel>* Init(void* ptr,
                                                             size_t num_bits) {
    // Relies on impl details of PrimitiveBitSet and MultiLevelBitSet.
    std::memset(ptr, 0, RequiredSize(num_bits));
    return reinterpret_cast<This*>(ptr);
  }

  void set(size_t pos, bool value = true) {
    assert(0 <= pos && pos < kMaxBits);

    size_t quot = pos / SecondLevel::kMaxBits;
    size_t rem = pos % SecondLevel::kMaxBits;
    SecondLevel* second_level = GetSecondLevel(quot);
    second_level->set(rem, value);

    bool full = second_level->countr_one() == SecondLevel::kMaxBits;
    first_level_.set(quot, full);
  }

  bool test(size_t pos) const {
    assert(0 <= pos && pos < kMaxBits);

    size_t quot = pos / SecondLevel::kMaxBits;
    size_t rem = pos % SecondLevel::kMaxBits;
    return GetSecondLevel(quot)->test(rem);
  }

  size_t countr_one() const {
    size_t first_level_ones = first_level_.countr_one();
    // If every second level block is full,
    // then subtract one from the index to avoid going out of bounds.
    //
    // We could alternatively return early here, but this way is *branchless*.
    size_t idx = first_level_ones == FirstLevel::kMaxBits ? first_level_ones - 1
                                                          : first_level_ones;
    size_t unfull_block_count = GetSecondLevel(idx)->countr_one();
    return idx * SecondLevel::kMaxBits + unfull_block_count;
  }

 private:
  SecondLevel* GetSecondLevel(size_t idx) {
    return reinterpret_cast<SecondLevel*>(
        &second_level_[kMaxSecondLevelSize * idx]);
  }

  const SecondLevel* GetSecondLevel(size_t idx) const {
    return reinterpret_cast<const SecondLevel*>(
        &second_level_[kMaxSecondLevelSize * idx]);
  }

  /**
   * A bitset where first_level_.test(i) indicates if second_level_[i]
   * is completely filled with ones.
   */
  FirstLevel first_level_;

  /**
   * The actual content of this bitset.
   *
   * Objects here are of type `SecondLevel`, but they're of variable length
   * generally.
   */
  uint8_t second_level_[];
};

template <typename T>
constexpr T ZeroIfFalse(bool pred, T value) {
  return (-static_cast<T>(pred)) & value;
}

/**
 * A single level bitset.
 */
template <PrimitiveBitsT Primitive, size_t N>
class PackedPrimitiveBitSet
    : public DefaultAllocable<PackedPrimitiveBitSet<Primitive, N>, size_t> {
 private:
  static constexpr auto kOne = static_cast<uint64_t>(1);
  static constexpr size_t kBitsPerPrimitive = sizeof(Primitive) * 8;

 public:
  static constexpr size_t kMaxBits = kBitsPerPrimitive * N;

  explicit PackedPrimitiveBitSet(size_t /*num_bits*/) {}

  void set(size_t pos, bool value = true) {
    size_t quot = pos / kBitsPerPrimitive;
    size_t rem = pos % kBitsPerPrimitive;
    bits_[quot] &= ~(kOne << rem);
    bits_[quot] |= static_cast<Primitive>(value) << rem;
  }

  bool test(size_t pos) const {
    size_t quot = pos / kBitsPerPrimitive;
    size_t rem = pos % kBitsPerPrimitive;
    return static_cast<bool>(bits_[quot] & (kOne << rem));
  }

  size_t popcount() const {
    size_t count = 0;
    for (uint64_t bit : bits_) {
      count += std::popcount(bit);
    }
    return count;
  }

  size_t countr_one() const {
    size_t res = 0;
    for (int i = 0; i < N; i++) {
      res += ZeroIfFalse(res == (i * kBitsPerPrimitive),
                         std::countr_one(bits_[i]));
    }
    return res;
  }

 private:
  Primitive bits_[N] = { 0 };
};

}  // namespace internal

using BitSet32 = internal::PrimitiveBitSet<uint32_t>;
using BitSet64 = internal::PrimitiveBitSet<uint64_t>;
using BitSet512 = internal::PackedPrimitiveBitSet<uint64_t, 8>;
using BitSet1024 = internal::MultiLevelBitSet<uint32_t, BitSet32>;
using BitSet4096 = internal::MultiLevelBitSet<uint64_t, BitSet64>;
using BitSet262144 = internal::MultiLevelBitSet<uint64_t, BitSet4096>;

/**
 * WOW is this ugly.
 */
template <size_t N>
class BitSet {
 public:
  BitSet() {
    T::Init(data_, N);
  }

  void set(size_t pos, bool value = true) {
    return reinterpret_cast<T*>(data_)->set(pos, value);
  }

  bool test(size_t pos) const {
    return reinterpret_cast<const T*>(data_)->test(pos);
  }

  size_t countr_one() const {
    return reinterpret_cast<const T*>(data_)->countr_one();
  }

 private:
  using T = std::conditional<
      N <= 32, BitSet32,
      typename std::conditional<
          N <= 64, BitSet64,
          typename std::conditional<
              N <= 1024, BitSet1024,
              typename std::conditional<
                  N <= 4096, BitSet4096,
                  typename std::conditional<N <= 262144, BitSet262144, void>::
                      // Death Star #2
                      //
                      //                      ::<type>::
                      //               ::<type::<type>::type>::
                      //        ::<type::<type::<type>::type>::type>::
                      // ::<type::<type::<type::<type>::type>::type>::type>::
                      //        ::<type::<type::<type>::type>::type>::
                      //               ::<type::<type>::type>::
                      //                      ::<type>::
                      //                     //
                      //                    //
                      //                   // *pew pew*
                      //                  //
                      //                 //
                      //                //    ...this time, it has no weakness
                      //               //
                      //              //
                      //             //
                      //        _d^^^^b_  Their cries echoed among The Force:
                      //      .d'      'b.
                      //     .p   HOF2   q.  *there must be a less violent way!
                      //      'q.      .p'  *nooo, not... Darth Template!*
                      //        ^q____p^   *ahhhhh arghhhh, why, oh why!*
                      //
                  type>::type>::type>::type>::type;

  uint8_t data_[T::RequiredSize(N)];
};

}  // namespace jsmalloc
