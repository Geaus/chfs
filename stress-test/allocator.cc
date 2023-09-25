#include <gtest/gtest.h>
#include <random>
#include <unordered_set>

#include "block/allocator.h"

namespace chfs {

TEST(BlockAllocatorTest, StressTest1) {
  const usize block_sz = 4096;
  const usize block_cnt = 1024 * 1024;
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(block_cnt, block_sz));

  auto allocator = BlockAllocator(bm);
  auto bitmap_block_cnt = allocator.total_bitmap_block();

  ASSERT_TRUE(bitmap_block_cnt > 0);

  //  const u64 test_iterations =
  //      static_cast<u64>(block_sz) * static_cast<u64>(block_cnt) * 2;
  const u64 test_iterations = 100000;
  ASSERT_TRUE(test_iterations > 0);

  std::unordered_set<block_id_t> active_blocks;
  std::mt19937 gen(0xdeadbeaf);
  std::uniform_int_distribution<> dis(0, 1);
  std::uniform_real_distribution<> dis1(0, 1);

  double threshold = 0.7;

  auto before_free_blocks = allocator.free_block_cnt();

  for (u64 i = 0; i < test_iterations; ++i) {
    if (active_blocks.empty() || dis1(gen) <= threshold) {
      // Allocate a block
      auto block = allocator.allocate();
      ASSERT_TRUE(block.is_ok());

      if (active_blocks.find(block.unwrap()) != active_blocks.end()) {
        std::cerr << "Block allocated twice for: " << block.unwrap()
                  << std::endl;
        FAIL();
      }
      active_blocks.insert(block.unwrap());

    } else {
      // Deallocate a random block
      auto it = active_blocks.begin();
      std::advance(it, dis(gen) % active_blocks.size());

      auto res = allocator.deallocate(*it);
      ASSERT_TRUE(res.is_ok());
      active_blocks.erase(it);
    }
  }

  // some final checks
  auto after_free_blocks = allocator.free_block_cnt();
  ASSERT_EQ(after_free_blocks + active_blocks.size(), before_free_blocks);
  std::cout << "Final check: active_blocks count: " << active_blocks.size()
            << std::endl;
}

} // namespace chfs

int main(int argc, char **argv) {
  // Initialize Google Test
  ::testing::InitGoogleTest(&argc, argv);

  // Run the tests
  return RUN_ALL_TESTS();
}