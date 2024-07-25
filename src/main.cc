#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>

// NOLINTNEXTLINE(misc-no-recursion)
double AllHealthy(uint32_t num_sheep, double p) {
  if (num_sheep == 0) {
    return 1;
  }
  if (num_sheep % 2 == 0) {
    double half_healthy = AllHealthy(num_sheep / 2, p);
    return half_healthy * half_healthy;
  }
  return (1 - p) * AllHealthy(num_sheep - 1, p);
}

// `p` is the probability of a positive test.
double ExpectedTests(uint32_t num_sheep, double p) {
  double arr[num_sheep * 2];
  std::memset(arr, 0, sizeof(arr));

  // 0 tests needed for one sheep where you know at least 1 is positive.
  arr[0] = 0;
  // 1 test needed for one sheep where you don't know if any are positive.
  arr[1] = 1;

  std::function<double&(uint32_t, bool)> mem = [&arr](uint32_t sheep,
                                                      bool one_is_sick) {
    return std::ref(arr[2 * (sheep - 1) + (one_is_sick ? 0 : 1)]);
  };

  uint32_t last_best_sick = 1;

  for (uint32_t sheep = 2; sheep <= num_sheep; sheep++) {
    uint32_t guy_thats_min = std::numeric_limits<uint32_t>::max();

    // Calculate how many tests it takes to find the sick sheep in a flock of
    // `sheep` sheep if you know one is sick.
    double best_p = std::numeric_limits<double>::max();
    double normalization = 1 - AllHealthy(sheep, p);
    for (uint32_t trial_sheep = last_best_sick;
         trial_sheep < std::min(last_best_sick + 2, sheep); trial_sheep++) {
      double if_trial_negative = mem(sheep - trial_sheep, true);
      double if_trial_positive =
          mem(trial_sheep, true) + mem(sheep - trial_sheep, false);
      double p_any_sick = (1 - AllHealthy(trial_sheep, p)) / normalization;
      double ev = 1 + (1 - p_any_sick) * if_trial_negative +
                  p_any_sick * if_trial_positive;

      if (ev < best_p) {
        guy_thats_min = trial_sheep;
        best_p = ev;
      }
    }
    mem(sheep, true) = best_p;
    last_best_sick = guy_thats_min;

    // Calculate how many tests is takes to find the sick sheep in a flock of
    // `sheep` sheep if you don't know if any are sick.

    // Helper function to calculate the expected number of tests given the size
    // of the first trial to use.
    auto calc_ev = [&mem, sheep, p](uint32_t trial_sheep) {
      double p_all_healthy = AllHealthy(trial_sheep, p);
      return 1 + (1 - p_all_healthy) * mem(trial_sheep, true) +
             mem(sheep - trial_sheep, false);
    };

    // This is the case where we test all sheep to see if they are healthy, and
    // if not then we are in the case from the beginning of the loop.
    best_p = 1 + (1 - AllHealthy(sheep, p)) * mem(sheep, true);
    // Only check integer divisions of the group.
    uint32_t div;
    for (div = 2; sheep / div != 0 && sheep / div != sheep / (div - 1); div++) {
      uint32_t trial_sheep = sheep / div;
      best_p = std::min(calc_ev(trial_sheep), best_p);
    }
    for (uint32_t trial_sheep = 1; trial_sheep < sheep / div; trial_sheep++) {
      best_p = std::min(calc_ev(trial_sheep), best_p);
    }
    mem(sheep, false) = best_p;
  }

  return mem(num_sheep, false);
}

int main() {
  const uint32_t sheep = 10000;
  double sum = 0;
  for (uint32_t num = 1; num <= 50; num++) {
    sum += ExpectedTests(sheep, static_cast<double>(num) / 100.);
  }
  std::cout << std::setprecision(12) << sum << std::endl;
  return 0;
}
