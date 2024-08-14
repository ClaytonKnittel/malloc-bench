#define MALLOC_ASSERT(cond) assert(cond)
#define MALLOC_ASSERT_EQ(lhs, rhs)                                          \
  do {                                                                      \
    auto __l = (lhs);                                                       \
    auto __r = (rhs);                                                       \
    if (__l != __r) {                                                       \
      std::cerr << __FILE__ << ":" << __LINE__ << ": Expected equality of " \
                << std::hex << __l << " and " << __r << std::endl;          \
    }                                                                       \
  } while (0)

// #define MALLOC_ASSERT(cond)