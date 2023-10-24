//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// macros.h
//
// Identification: src/include/common/macros.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace chfs {

#define CHFS_ASSERT(expr, message) assert((expr) && (message))
#define CHFS_VERIFY(expr, message)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::cerr << "Chfs fatal invariant check failed: " << (message)          \
                << " at " << __FILE__ << ":" << __LINE__ << std::endl;         \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#define UNIMPLEMENTED()                                                        \
  do {                                                                         \
    std::cerr << "Unimplemented function of chfs: " << __PRETTY_FUNCTION__     \
              << " at " << __FILE__ << ":" << __LINE__ << std::endl;           \
    std::abort();                                                              \
  } while (0)

// Macros to disable copying and moving
#define DISALLOW_COPY(cname)                                                   \
  cname(const cname &) = delete;                   /* NOLINT */                \
  auto operator=(const cname &)->cname & = delete; /* NOLINT */

#define DISALLOW_MOVE(cname)                                                   \
  cname(cname &&) = delete;                   /* NOLINT */                     \
  auto operator=(cname &&)->cname & = delete; /* NOLINT */

#define DISALLOW_COPY_AND_MOVE(cname)                                          \
  DISALLOW_COPY(cname);                                                        \
  DISALLOW_MOVE(cname);

// Macros for block computation
#define ROUND_UP(x, n) (((x) + (n)-1) & ~((n)-1))
#define ROUND_DOWN(x, n) ((x) & ~((n)-1))

} // namespace chfs
