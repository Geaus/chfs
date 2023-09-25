#include <random>

#include "./common.h"
#include "filesystem/operations.h"
#include "gtest/gtest.h"

namespace chfs {

TEST(FileSystemTest, CreateAndGetAttr) {
  std::mt19937 rng(get_test_seed());
  std::uniform_int_distribution<int> uni(0, 100);

  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);

  for (usize i = 0; i < kFileNum; i++) {
    int random_num = uni(rng);
    if (random_num < 3) {
      // create a directory
      auto res = fs.alloc_inode(InodeType::Directory);
      ASSERT_TRUE(res.is_ok());

      auto type = fs.gettype(res.unwrap());
      ASSERT_TRUE(type.is_ok());

      ASSERT_EQ(type.unwrap(), InodeType::Directory);
    } else {
      auto res = fs.alloc_inode(InodeType::FILE);
      ASSERT_TRUE(res.is_ok());

      auto type = fs.gettype(res.unwrap());
      ASSERT_TRUE(type.is_ok());

      ASSERT_EQ(type.unwrap(), InodeType::FILE);
    }
  }
}

} // namespace chfs