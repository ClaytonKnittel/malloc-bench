#pragma once

#ifdef __has_attribute
#define BENCH_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define BENCH_HAS_ATTRIBUTE(x) 0
#endif

#if BENCH_HAS_ATTRIBUTE(always_inline) || \
    (defined(__GNUC__) && !defined(__clang__))
#define BENCH_ALWAYS_INLINE __attribute__((always_inline))
#else
#define BENCH_ALWAYS_INLINE
#endif

#if BENCH_HAS_ATTRIBUTE(guarded_by)
#define BENCH_GUARDED_BY(mu) __attribute__((guarded_by(mu)))
#else
#define BENCH_GUARDED_BY(mu)
#endif

#if BENCH_HAS_ATTRIBUTE(exclusive_locks_required)
#define BENCH_EXCLUSIVE_LOCKS_REQUIRED(...) \
  __attribute__((exclusive_locks_required(__VA_ARGS__)))
#else
#define BENCH_EXCLUSIVE_LOCKS_REQUIRED(...)
#endif

#if BENCH_HAS_ATTRIBUTE(locks_excluded)
#define BENCH_LOCKS_EXCLUDED(...) __attribute__((locks_excluded(__VA_ARGS__)))
#else
#define BENCH_LOCKS_EXCLUDED(...)
#endif
