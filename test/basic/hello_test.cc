#include "gtest/gtest.h"

namespace chfs {

auto add(int a, int b) -> int { return a + b; }

TEST(BasicTest, Hello) { EXPECT_EQ(-6, add(-3, -3)); }

} // namespace chfs