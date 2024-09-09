#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace jsmalloc {

/** A type that can be dynamically allocated. */
template <typename T, typename InitArg>
concept AllocableT = requires(void* ptr, InitArg arg) {
  { T::RequiredSize(arg) } -> std::same_as<size_t>;
  { T::Init(ptr, arg) } -> std::same_as<T*>;
};

template <typename B>
concept BitSetT =
    requires(B bitset, size_t pos, bool value, void* ptr, size_t num_bits) {
      { bitset.set(pos, value) } -> std::same_as<void>;
      { bitset.set(pos) } -> std::same_as<void>;
      { bitset.test(pos) } -> std::same_as<bool>;
      { bitset.full() } -> std::same_as<bool>;
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
template <PrimitiveBitsT T = uint64_t, size_t N = sizeof(T) * 8>
class PrimBitSet {
 public:
  static constexpr size_t kMaxBits = sizeof(T) * 8;

 private:
  static constexpr auto kOne = static_cast<T>(1);
  static constexpr size_t kFullBits = (-kOne) >> (kMaxBits - N);

 public:
  void set(size_t pos, bool value = true) {
    bits_ &= ~(kOne << pos);
    bits_ |= static_cast<T>(value) << pos;
  }

  bool test(size_t pos) const {
    return static_cast<bool>(bits_ & (kOne << pos));
  }

  bool full() const {
    return bits_ == kFullBits;
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

static_assert(BitSetT<PrimBitSet<uint64_t>>);

/**
 * A multi-level bitset.
 *
 * Allows composing BitSetT classes to an arbitrary depth,
 * while supporting logarithmic countr_one.
 */
template <PrimitiveBitsT P, template <size_t> class SecondLevel,
          size_t N = PrimBitSet<P>::kMaxBits * SecondLevel<1>::kMaxBits>
requires BitSetT<SecondLevel<1>>
class MultiLevelBitSet {
 private:
  using FirstLevel = PrimBitSet<P>;
  static constexpr size_t kSecondLevelMaxBits = SecondLevel<1>::kMaxBits;
  static constexpr size_t kSecondLevelLen =
      (N + kSecondLevelMaxBits - 1) / kSecondLevelMaxBits;

  static_assert(N % kSecondLevelLen == 0);
  static constexpr size_t kBitsPerSecondLevel = N / kSecondLevelLen;

 public:
  static constexpr size_t kMaxBits =
      PrimBitSet<P>::kMaxBits * kSecondLevelMaxBits;
  static_assert(N <= kMaxBits);

  void set(size_t pos, bool value = true) {
    assert(pos >= 0);
    assert(pos < kMaxBits);

    size_t quot = pos / kBitsPerSecondLevel;
    size_t rem = pos % kBitsPerSecondLevel;
    second_level_[quot].set(rem, value);
    first_level_.set(quot, second_level_[quot].full());
  }

  bool test(size_t pos) const {
    assert(pos >= 0);
    assert(pos < kMaxBits);

    size_t quot = pos / kBitsPerSecondLevel;
    size_t rem = pos % kBitsPerSecondLevel;
    return second_level_[quot].test(rem);
  }

  bool full() const {
    return first_level_.full();
  }

  size_t countr_one() const {
    size_t first_level_ones = first_level_.countr_one();

    // These ternaries compile to cmov's.
    //
    // It's probably better to just branch here instead.
    // Most of the time a bitset will not be full anyway.
    //
    // This is more fun.
    bool full = first_level_ones == kSecondLevelLen;
    size_t first_level_count = first_level_ones * kBitsPerSecondLevel;
    size_t idx = full ? 0 : first_level_ones;
    size_t remainder_block_count = second_level_[idx].countr_one();
    return full ? first_level_count : first_level_count + remainder_block_count;
  }

 private:
  /**
   * A bitset where first_level_.test(i) indicates if content_[i]
   * is completely filled with ones.
   */
  PrimBitSet<P, kSecondLevelLen> first_level_;

  /**
   * The actual content of this bitset, split into blocks
   * that hold an equal number of bits.
   */
  SecondLevel<kBitsPerSecondLevel> second_level_[kSecondLevelLen];
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

namespace internal {

template <size_t N>
using BitSet64 = PrimBitSet<uint64_t, N>;

template <size_t N>
using BitSet512 = MultiLevelBitSet<uint32_t, BitSet64, N>;

template <size_t N>
using BitSet4096 = MultiLevelBitSet<uint64_t, BitSet64, N>;

template <size_t N>
using BitSet262144 = MultiLevelBitSet<uint64_t, BitSet4096, N>;

}  // namespace internal

/** A bitset that supports up to 2^6 bits. */
template <size_t N>
using BitSet64 = internal::BitSet64<N>;

/** A bitset that supports up to 2^9 bits. */
template <size_t N>
using BitSet512 = PopCountBitSet<internal::BitSet512<N>>;

/** A bitset that supports up to 2^12 bits. */
template <size_t N>
using BitSet4096 = PopCountBitSet<internal::BitSet4096<N>>;

/** A bitset that supports up to 2^18 bits. */
template <size_t N>
using BitSet262144 = PopCountBitSet<internal::BitSet262144<N>>;

static_assert(sizeof(BitSet64<1>) == 8);
static_assert(sizeof(BitSet64<64>) == 8);

static_assert(sizeof(BitSet4096<1>) == 24);
static_assert(sizeof(BitSet512<480>) == 80);
static_assert(sizeof(BitSet4096<4096>) == 528);

static_assert(sizeof(BitSet262144<1>) == 32);
static_assert(sizeof(BitSet262144<262144>) == 33296);

}  // namespace jsmalloc
