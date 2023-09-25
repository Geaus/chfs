#include "common/macros.h"
#include "metadata/superblock.h"
#include "gtest/gtest.h"

#include <cstring>

namespace chfs {

class SuperblockTest : public ::testing::Test {
protected:
  static std::shared_ptr<BlockAllocator> allocator;
  static std::shared_ptr<BlockManager> bm;
  // This function is called before every test.
  static void SetUpTestCase() {
    const usize block_sz = 4096;

    // we use a smaller block cnt to reduce the test time
    // more complex tests are placed in the stress-test/allocator.cc
    const usize block_cnt = 1024;
    bm = std::shared_ptr<BlockManager>(new BlockManager(block_cnt, block_sz));

    // The 1 stands for reserve a block for the superblock
    allocator = std::shared_ptr<BlockAllocator>(new BlockAllocator(bm, 1));
  }

  void SetUp() override {
    auto superblock = SuperBlock(bm, 1024);
    superblock.flush(0).unwrap();
  }

  // This function is called after every test.
  void TearDown() override{
      // remove("test.db");
  };
};

std::shared_ptr<BlockAllocator> SuperblockTest::allocator = nullptr;
std::shared_ptr<BlockManager> SuperblockTest::bm = nullptr;

// NOLINTNEXTLINE
// Test creating a superblock from zero
TEST_F(SuperblockTest, CreateFromZero) {
  auto superblock1 = SuperBlock::create_from_existing(bm, 0).unwrap();
  ASSERT_EQ(superblock1->get_ninodes(), 1024);
}

} // namespace chfs