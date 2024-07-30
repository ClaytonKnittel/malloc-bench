#pragma once

#include <cstdlib>
#include <iostream>

#define CK_ASSERT(cond)                                                     \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::cerr << __FILE__ ":" << __LINE__ << ": Condition failed: " #cond \
                << std::endl;                                               \
      abort();                                                              \
    }                                                                       \
  } while (0)
