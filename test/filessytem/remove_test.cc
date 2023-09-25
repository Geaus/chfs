#include <random>

#include "./common.h"
#include "filesystem/operations.h"
#include "gtest/gtest.h"

namespace chfs {

TEST(FileSystemTest, Remove) {
  // The first part is very the same as the create_and_getter test
  std::mt19937 rng(get_test_seed());
  std::uniform_int_distribution<int> uni(0, 100);
  std::vector<inode_id_t> id_list;

  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);
  auto free_block_cnt = fs.get_free_blocks_num().unwrap();
  ASSERT_GE(free_block_cnt, 0);

  for (usize i = 0; i < kFileNum; i++) {
    int random_num = uni(rng);
    if (random_num < 3) {
      // create a directory
      auto res = fs.alloc_inode(InodeType::Directory);
      ASSERT_TRUE(res.is_ok());

      auto type = fs.gettype(res.unwrap());
      ASSERT_TRUE(type.is_ok());

      ASSERT_EQ(type.unwrap(), InodeType::Directory);

      id_list.push_back(res.unwrap());
    } else {
      auto res = fs.alloc_inode(InodeType::FILE);
      ASSERT_TRUE(res.is_ok());

      auto type = fs.gettype(res.unwrap());
      ASSERT_TRUE(type.is_ok());

      ASSERT_EQ(type.unwrap(), InodeType::FILE);

      id_list.push_back(res.unwrap());
    }
  }
  auto free_block_cnt_after_1 = fs.get_free_blocks_num().unwrap();
  ASSERT_LE(free_block_cnt_after_1, free_block_cnt);

  // The real test remove here
  for (auto id : id_list) {
    auto res = fs.remove_file(id);
    ASSERT_TRUE(res.is_ok());

    // After remove, the getattr should never be ok
    auto res1 = fs.getattr(id);
    ASSERT_FALSE(res1.is_ok());
  }

  auto free_block_cnt_after_2 = fs.get_free_blocks_num().unwrap();
  ASSERT_EQ(free_block_cnt_after_2, free_block_cnt);
}

} // namespace chfs