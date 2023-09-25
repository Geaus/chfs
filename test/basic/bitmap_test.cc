#include "gtest/gtest.h"

#include "common/bitmap.h"
#include "common/macros.h"

namespace chfs {

TEST(BasicTest, BitmapBasic) {
  usize data_sz = 4096;
  auto data = new u8[data_sz];

  auto bm = Bitmap(data, data_sz);
  bm.zeroed();

  for (usize i = 0; i < data_sz * KBitsPerByte; i++) {
    EXPECT_FALSE(bm.check(i));
  }

  bm.set(3);
  EXPECT_TRUE(bm.check(3));

  bm.set(50);
  EXPECT_TRUE(bm.check(50));

  bm.clear(3);
  EXPECT_FALSE(bm.check(3));

  // Check bit 3 after clear
  EXPECT_TRUE(bm.check(50));
  delete[] data;
}

TEST(BasicTest, BitmapFree) {
  usize data_sz = 4096;
  auto data = new u8[data_sz];

  auto bm = Bitmap(data, data_sz);
  bm.zeroed();

  for (usize i = 0; i < data_sz * KBitsPerByte; i++) {
    auto free = bm.find_first_free();
    if (free) {
      EXPECT_EQ(free.value(), i);
      bm.set(i);
    } else {
      std::cerr << "failed on " << i << std::endl;
      FAIL();
    }

    if (i % 1000 == 0) {
      EXPECT_EQ(bm.count_ones(), i + 1);
    }
  }

  bm.clear(4096);
  EXPECT_TRUE(bm.find_first_free().has_value());
  bm.set(4096);
  EXPECT_FALSE(bm.find_first_free().has_value());

  delete[] data;
}

} // namespace chfs