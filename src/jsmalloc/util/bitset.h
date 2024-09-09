#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

namespace jsmalloc {
namespace internal {

template <typename B>
concept BitSetT =
    requires(B bitset, size_t pos, bool value, void* ptr, size_t num_bits) {
      { bitset.set(pos, value) } -> std::same_as<void>;
      { bitset.set(pos) } -> std::same_as<void>;
      { bitset.test(pos) } -> std::same_as<bool>;
      { bitset.countr_one() } -> std::same_as<size_t>;
      { B::kBits } -> std::convertible_to<size_t>;
    };

template <typename T>
concept PrimitiveBitsT =
    (std::same_as<T, uint8_t> || std::same_as<T, uint16_t> ||
     std::same_as<T, uint32_t> || std::same_as<T, uint64_t>);

/**
 * A bitset with a primitive backing.
 */
template <PrimitiveBitsT T = uint64_t>
class PrimitiveBitSet {
 public:
  static constexpr size_t kBits = sizeof(T) * 8;

 private:
  static constexpr auto kOne = static_cast<T>(1);

 public:
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
template <BitSetT FirstLevel, BitSetT SecondLevel,
          size_t N = FirstLevel::kBits * SecondLevel::kBits>
class MultiLevelBitSet {
 private:
  static constexpr size_t kSecondLevelLen =
      (N + SecondLevel::kBits - 1) / SecondLevel::kBits;

 public:
  static constexpr size_t kBits = FirstLevel::kBits * SecondLevel::kBits;

  void set(size_t pos, bool value = true) {
    assert(0 <= pos && pos < kBits);

    size_t quot = pos / SecondLevel::kBits;
    size_t rem = pos % SecondLevel::kBits;
    second_level_[quot].set(rem, value);

    bool full = second_level_[quot].countr_one() == SecondLevel::kBits;
    first_level_.set(quot, full);
  }

  bool test(size_t pos) const {
    assert(0 <= pos && pos < kBits);

    size_t quot = pos / SecondLevel::kBits;
    size_t rem = pos % SecondLevel::kBits;
    return second_level_[quot].test(rem);
  }

  size_t countr_one() const {
    size_t first_level_ones = first_level_.countr_one();
    // If every second level block is full,
    // then subtract one from the index to avoid going out of bounds.
    //
    // We could alternatively return early here, but this way is *branchless*.
    size_t idx = first_level_ones == FirstLevel::kBits ? first_level_ones - 1
                                                       : first_level_ones;
    size_t unfull_block_count = second_level_[idx].countr_one();
    return idx * SecondLevel::kBits + unfull_block_count;
  }

 private:
  /**
   * A bitset where first_level_.test(i) indicates if content_[i]
   * is completely filled with ones.
   */
  FirstLevel first_level_;

  /**
   * The actual content of this bitset, split into blocks
   * that hold an equal number of bits.
   */
  SecondLevel second_level_[kSecondLevelLen];
};

/** Wrapper around a bitset that provides `std::popcount` functionality. */
template <BitSetT BitSet>
class PopCountBitSet : public BitSet {
 public:
  void set(size_t pos, bool value = true) {
    popcount_ -= static_cast<size_t>(BitSet::test(pos));
    BitSet::set(pos, value);
    popcount_ += static_cast<size_t>(value);
  }

  size_t popcount() {
    return popcount_;
  }

 private:
  size_t popcount_ = 0;
};

using BitSet32 = PrimitiveBitSet<uint32_t>;
using BitSet64 = PrimitiveBitSet<uint64_t>;

template <size_t N>
using BitSet4096NoPopCount = MultiLevelBitSet<BitSet64, BitSet64, N>;

template <size_t N>
using BitSet262144NoPopCount =
    MultiLevelBitSet<BitSet64, BitSet4096NoPopCount<4096>, N>;

}  // namespace internal

using BitSet32 = internal::BitSet32;
using BitSet64 = internal::BitSet64;

/** A bitset that supports up to 2^12 bits. */
template <size_t N>
using BitSet4096 = internal::PopCountBitSet<internal::BitSet4096NoPopCount<N>>;

/** A bitset that supports up to 2^18 bits. */
template <size_t N>
using BitSet262144 =
    internal::PopCountBitSet<internal::BitSet262144NoPopCount<N>>;

/** A bitset that statically selects one of the above bitsets. */
template <size_t N>
using BitSet = std::conditional<
    N <= 32, internal::BitSet32,
    typename std::conditional<
        N <= 64, internal::BitSet64,
        typename std::conditional<N <= 4096, BitSet4096<N>,
                                  BitSet262144<N>>::type>::type>::type;

}  // namespace jsmalloc
