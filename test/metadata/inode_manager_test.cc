#include "common/macros.h"
#include "metadata/manager.h"
#include "metadata/superblock.h"

#include "gtest/gtest.h"
#include <cstring>

namespace chfs {

const usize test_block_cnt = 4096;
const usize test_block_sz = 4096;

class InodeManagerTest : public ::testing::Test {
protected:
  u64 test_inode_num;
  std::shared_ptr<BlockManager> bm;
  std::shared_ptr<InodeManager> inode_manager;

  // This function is called before every test.
  void SetUp() override {
    bm = std::shared_ptr<BlockManager>(
        new BlockManager(test_block_cnt, test_block_sz));
    test_inode_num = 1024;
    inode_manager =
        std::shared_ptr<InodeManager>(new InodeManager(bm, test_inode_num));
    ASSERT_GE(inode_manager->get_max_inode_supported(), test_inode_num);
    test_inode_num = inode_manager->get_max_inode_supported();
  }

  // This function is called after every test.
  void TearDown() override{};
};

// NOLINTNEXTLINE
// Test creating a superblock from zero
TEST_F(InodeManagerTest, InitAndTable) {
  // test inode table
  for (inode_id_t i = 1; i < 64; ++i) {
    inode_manager->set_table(i - 1, i + 73).unwrap();
    ASSERT_EQ(inode_manager->get(i).unwrap(), i + 73);
  }
  test_inode_num = inode_manager->get_max_inode_supported();
}

TEST_F(InodeManagerTest, Allocation) {
  auto inode_manager1 =
      InodeManager::create_from_block_manager(bm, test_inode_num).unwrap();
  ASSERT_EQ(inode_manager1.get_max_inode_supported(), test_inode_num);

  auto free_inode_cnt = inode_manager1.free_inode_cnt().unwrap();
  auto allocator = BlockAllocator(bm, inode_manager1.get_reserved_blocks());

  ASSERT_EQ(free_inode_cnt, test_inode_num);

  block_id_t prev_id = 0;
  // now start allocate an inode
  for (usize i = 0; i < free_inode_cnt; ++i) {
    auto free_block_res = allocator.allocate();
    if (free_block_res.is_err()) {
      ASSERT_EQ(prev_id, test_block_cnt - 1);
      break;
    }
    auto free_block = free_block_res.unwrap();
    auto inode_id =
        inode_manager1.allocate_inode(InodeType::Directory, free_block)
            .unwrap();
    ASSERT_EQ(inode_id, i + 1);
    prev_id = free_block;
  }
}

} // namespace chfs