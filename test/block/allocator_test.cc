#include "block/allocator.h"
#include "common/macros.h"
#include "gtest/gtest.h"

namespace chfs {

class BlockAllocatorTest : public ::testing::Test {
protected:
  // This function is called before every test.
  void SetUp() override {}

  // This function is called after every test.
  void TearDown() override{};
};

// We leave the allocator test to a separate directory
TEST_F(BlockAllocatorTest, Init) {
  const usize block_sz = 4096;
  const usize block_cnt = 1024 * 1024;

  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(block_cnt, block_sz));

  auto allocator = BlockAllocator(bm);
  auto bitmap_block_cnt = allocator.total_bitmap_block();
  EXPECT_EQ(allocator.free_block_cnt(), bm->total_blocks() - bitmap_block_cnt);

  auto bm1 =
      std::shared_ptr<BlockManager>(new BlockManager(block_cnt, block_sz));

  // we reserve the first blocks for other purpose
  usize reserved_block = 1;
  auto allocator_1 = BlockAllocator(bm1, reserved_block);
  auto bitmap_block_cnt1 = allocator_1.total_bitmap_block();
  EXPECT_EQ(allocator_1.free_block_cnt(),
            bm1->total_blocks() - bitmap_block_cnt1 - reserved_block);
  EXPECT_EQ(allocator_1.free_block_cnt() + 1, allocator.free_block_cnt());
}

TEST_F(BlockAllocatorTest, Allocation) {
  const usize block_sz = 4096;

  // we use a smaller block cnt to reduce the test time
  // more complex tests are placed in the stress-test/allocator.cc
  const usize block_cnt = 1024;
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(block_cnt, block_sz));

  auto allocator = BlockAllocator(bm);
  auto bitmap_block_cnt = allocator.total_bitmap_block();

  // test the allocation
  auto free_block_cnt = allocator.free_block_cnt();
  for (usize i = 0; i < free_block_cnt; i++) {
    auto block = allocator.allocate();
    EXPECT_TRUE(block.is_ok());
    auto free_block_id = block.unwrap();
    EXPECT_EQ(free_block_id, i + bitmap_block_cnt);
  }

  ASSERT_EQ(allocator.free_block_cnt(), 0);

  // now test the free
  for (usize i = 0; i < free_block_cnt; i++) {
    EXPECT_TRUE(allocator.deallocate(i + bitmap_block_cnt).is_ok());
  }
}

} // namespace chfs