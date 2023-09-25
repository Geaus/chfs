#include "block/manager.h"
#include "common/macros.h"
#include "gtest/gtest.h"
#include <cstring>

namespace chfs {

class BlockManagerTest : public ::testing::Test {
protected:
  // This function is called before every test.
  void SetUp() override {
    // remove("test.db");
  }

  // This function is called after every test.
  void TearDown() override{
      // remove("test.db");
  };
};

// NOLINTNEXTLINE
TEST_F(BlockManagerTest, ReadWritePageTest) {
  std::string file("test.db");
  auto bm = BlockManager(file);

  u8 *buf = new u8[bm.block_size()];
  u8 *data = new u8[bm.block_size()];

  // FIXME: what if sizeof(u8) != sizeof(char)?
  std::strncpy((char *)data, "A test string.", bm.block_size());

  bm.write_block(0, data);
  bm.read_block(0, buf);
  EXPECT_EQ(std::memcmp(buf, data, bm.block_size()), 0);

  delete[] buf;
  delete[] data;
}

TEST_F(BlockManagerTest, ZeroTest) {
  std::string file("test.db");
  auto bm = BlockManager(file);

  u8 *buf = new u8[bm.block_size()];
  u8 *data = new u8[bm.block_size()];

  std::strncpy((char *)data, "A test string.", bm.block_size());

  bm.write_block(1, data);
  bm.read_block(1, buf);
  EXPECT_EQ(std::memcmp(buf, data, bm.block_size()), 0);

  bool pre_check = false;
  for (usize i = 0; i < bm.block_size(); i++) {
    if (buf[i] != 0) {
      pre_check = true;
      break;
    }
  }
  ASSERT_EQ(pre_check, true);

  bm.zero_block(1);
  bm.read_block(1, buf);
  for (usize i = 0; i < bm.block_size(); i++) {
    EXPECT_EQ(buf[i], 0);
  }

  delete[] buf;
  delete[] data;
}

TEST_F(BlockManagerTest, InMemoryTest) {
  auto bm = BlockManager(4096, 4096);

  u8 *buf = new u8[bm.block_size()];
  u8 *data = new u8[bm.block_size()];

  // FIXME: what if sizeof(u8) != sizeof(char)?
  std::strncpy((char *)data, "A test string.", bm.block_size());

  bm.write_block(0, data);
  bm.read_block(0, buf);
  EXPECT_EQ(std::memcmp(buf, data, bm.block_size()), 0);

  delete[] buf;
  delete[] data;
}

TEST_F(BlockManagerTest, Iterator) {
  // 1024: block cnt
  // 4096: block size
  auto bm = BlockManager(1024, 4096);
  auto iter = BlockIterator::create(&bm, 2, 128).unwrap();

  usize count = 0;
  for (; iter.has_next(); iter.next(4096).unwrap()) {
    *iter.unsafe_get_value_ptr<u64>() = count + 73;
    iter.flush_cur_block().unwrap();
    count += 1;
  }
  ASSERT_EQ(count, 128 - 2);

  // re-test setted value
  for (uint i = 2; i < 128; ++i) {
    std::vector<u8> buffer(4096);
    bm.read_block(i, buffer.data());
    u64 *ptr = reinterpret_cast<u64 *>(buffer.data());
    ASSERT_EQ(*ptr, i - 2 + 73);
  }
}

} // namespace chfs
