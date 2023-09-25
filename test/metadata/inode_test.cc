#include "common/macros.h"
#include "metadata/inode.h"
#include "gtest/gtest.h"
#include <cstring>

namespace chfs {

#define TEST_BLOCK_SZ 4096

class InodeTest : public ::testing::Test {
protected:
  u8 test_inode_block[TEST_BLOCK_SZ];

  // This function is called before every test.
  void SetUp() override { memset(test_inode_block, 0, TEST_BLOCK_SZ); }

  // This function is called after every test.
  void TearDown() override{};
};

// NOLINTNEXTLINE
// Test creating a superblock from zero
TEST_F(InodeTest, Init) {
  auto inode = Inode(InodeType::FILE, TEST_BLOCK_SZ);
  ASSERT_TRUE(inode.get_nblocks() > 0);
  inode.flush_to_buffer(test_inode_block);

  auto inode_p = reinterpret_cast<Inode *>(test_inode_block);
  ASSERT_EQ(inode_p->get_type(), InodeType::FILE);
  ASSERT_EQ(inode.get_nblocks(), inode_p->get_nblocks());

  for (uint i = 0; i < inode_p->get_nblocks(); ++i) {
    ASSERT_EQ(inode_p->blocks[i], KInvalidBlockID);
  }

  // check file size
  auto file_sz_supported_by_one_block =
      static_cast<u64>(TEST_BLOCK_SZ / sizeof(block_id_t)) *
      static_cast<u64>(TEST_BLOCK_SZ);
  auto file_in_inode =
      static_cast<u64>((TEST_BLOCK_SZ - sizeof(Inode)) / sizeof(block_id_t) -
                       1) * // 1: the indirect block
      static_cast<u64>(TEST_BLOCK_SZ);
  ASSERT_EQ(inode.max_file_sz_supported(),
            file_sz_supported_by_one_block + file_in_inode);
}

TEST_F(InodeTest, Iteration) {
  auto inode_p = reinterpret_cast<Inode *>(test_inode_block);

  usize count = 0;
  for (auto iter = inode_p->begin(); iter != inode_p->end(); iter++) {
    ASSERT_EQ(*iter, KInvalidBlockID);
    count += 1;
  }
  ASSERT_EQ(count, inode_p->get_nblocks());

  count = 0;
  // Do some writes to check later
  for (auto iter = inode_p->begin(); iter != inode_p->end(); iter++) {
    *iter = count + 73;
  }
}

TEST_F(InodeTest, IterationWrite) {
  auto inode_p = reinterpret_cast<Inode *>(test_inode_block);

  usize count = 0;
  for (auto iter = inode_p->begin(); iter != inode_p->end(); iter++) {
    ASSERT_EQ(*iter, count + 73);
    count += 1;
  }
  ASSERT_EQ(count, inode_p->get_nblocks());
}

} // namespace chfs