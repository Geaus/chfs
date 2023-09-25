#include "./common.h"
#include "filesystem/operations.h"
#include "gtest/gtest.h"

namespace chfs {

TEST(BasicFileSystemTest, Init) {
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);

  auto total_nblocks = 32768;
  auto reserved_blocks =
      1 + 128 + 2 + 8; // superblock + inode table + inode bitmap + block bitmap
  ASSERT_EQ(fs.get_free_blocks_num().unwrap(), total_nblocks - reserved_blocks);

  // try allocate a block
  {
    auto res = fs.alloc_inode(InodeType::FILE);
    ASSERT_TRUE(res.is_ok());
    ASSERT_EQ(res.unwrap(), 1);
  }

  auto fs1 = FileOperation::create_from_raw(bm).unwrap();
  // +1: we have allocated a block for the inode before
  ASSERT_EQ(fs1->get_free_blocks_num().unwrap() + 1,
            total_nblocks - reserved_blocks);

  std::cout << "Basic FS test done" << std::endl;
}

} // namespace chfs