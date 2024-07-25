#pragma once

#include <cstdint>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "absl/status/statusor.h"

#define RAND_FILE_PATH "/dev/urandom"

namespace util {

class Rng {
 public:
  static absl::StatusOr<Rng> InitFromHw() {
    uint64_t rand_seed[2];

    int rand_fd = open(RAND_FILE_PATH, O_RDONLY);
    if (rand_fd < 0) {
      return absl::InternalError("Failed to open random file \"" RAND_FILE_PATH
                                 "\"");
    }

    ssize_t n_bytes =
        read(rand_fd, static_cast<void*>(&rand_seed), sizeof(rand_seed));
    if (n_bytes != sizeof(rand_seed)) {
      return absl::InternalError(
          "Failed to read 16 bytes from \"" RAND_FILE_PATH "\"");
    }

    close(rand_fd);

    return Rng(rand_seed[0], rand_seed[1]);
  }

  constexpr Rng(uint64_t seed, uint64_t seq_num)
      : state_(0), seq_num_((seq_num << 1LU) | 1LU) {
    GenRand();
    state_ += seed;
    GenRand();
  }

  // generates next random number in sequence
  constexpr uint32_t GenRand() {
    uint64_t prev = state_;
    state_ = prev * 6364136223846793005ULL + seq_num_;

    // do some xor stuff
    uint32_t x_or = ((prev >> 18U) ^ prev) >> 27U;
    uint32_t rot = prev >> 59U;

    // rotate result by "rot"
    return (x_or >> rot) | (x_or << ((-rot) & 0x1f));
  }

  // same as gen_rand, but gives a 64-bit number
  constexpr uint64_t GenRand64() {
    uint32_t r1 = GenRand();
    uint32_t r2 = GenRand();
    return (static_cast<uint64_t>(r1) << 32) | static_cast<uint64_t>(r2);
  }

  // generates a random number from 0 to max - 1
  constexpr uint32_t GenRandRange(uint32_t max) {
    // equivalent to 0x100000000lu % max, but is done with 32-bit numbers so
    // it's faster
    uint32_t thresh = -max % max;

    // range is limited to thresh and above, to eliminate any bias (i.e. if
    // max is 3, then 0 is not allowed to be chosen, as 0xffffffffffffffff
    // would also give 0 as a result, meaning 0 is slightly more likely to
    // be chosen)
    uint32_t res = 0;
    do {
      res = GenRand();
    } while (__builtin_expect(static_cast<uint64_t>(res < thresh), 0) != 0);

    return res % max;
  }

  // generates a random number from 0 to max - 1
  constexpr uint64_t GenRandRange64(uint64_t max) {
    // mathematically equivalent to 0x10000000000000000lu % max
    uint64_t thresh = -max % max;

    // range is limited to thresh and above, to eliminate any bias (i.e. if
    // max is 3, then 0 is not allowed to be chosen, as 0xffffffffffffffff
    // would also give 0 as a result, meaning 0 is slightly more likely to
    // be chosen)
    uint64_t res = 0;
    do {
      res = GenRand64();
    } while (__builtin_expect(static_cast<int64_t>(res < thresh), 0) != 0);

    return res % max;
  }

 private:
  // tracks state of RNG
  uint64_t state_;
  // sequence number for RNG (will determine how to get from one state
  // to the next, generates unique sequences for different initial states)
  uint64_t seq_num_;
};

}  // namespace util
