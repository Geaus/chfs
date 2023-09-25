#include <random>

#include "./common.h"
#include "filesystem/operations.h"
#include "gtest/gtest.h"

namespace chfs {

auto generate_random_string(std::mt19937 &rng) -> std::vector<u8> {
  std::uniform_int_distribution<uint> uni_sz(KLargeFileMin, KLargeFileMax);
  std::uniform_int_distribution<u8> uni_char(0, 26);

  auto sz = uni_sz(rng);
  std::vector<u8> data(sz);

  for (uint i = 0; i < sz; ++i) {
    data[i] = uni_char(rng) + 97;
  }

  return data;
}

auto vec_equal(std::vector<u8> &a, std::vector<u8> &b) -> bool {
  if (a.size() != b.size()) {
    return false;
  }

  for (uint i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) {
      std::cerr << "check error at [" << i << "]: " << (char)a[i] << " vs "
                << (char)b[i] << std::endl;
      return false;
    }
  }
  return true;
}

TEST(FileSystemTest, WriteLargeFile) {
  std::mt19937 rng(get_test_seed());

  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);

  std::unordered_map<inode_id_t, std::vector<u8>> contents;

  std::vector<inode_id_t> id_list;
  for (usize i = 0; i < kFileNum; i++) {
    // for (usize i = 0; i < 1; i++) {
    auto res = fs.alloc_inode(InodeType::FILE);
    ASSERT_TRUE(res.is_ok());
    id_list.push_back(res.unwrap());
  }

  // std::vector<u8> test_file_content(KLargeFileMax);
  for (uint i = 0; i < 10; ++i) {
    // TODO: initialize the test_file_content
    for (auto id : id_list) {
      contents[id] = generate_random_string(rng);

      auto res = fs.write_file(id, contents[id]);
      ASSERT_TRUE(res.is_ok());
    }

    for (auto id : id_list) {
      auto res = fs.read_file(id);
      ASSERT_TRUE(res.is_ok());
      auto res_data = res.unwrap();

      auto check = vec_equal(res_data, contents[id]);
      ASSERT_TRUE(check);
    }
  }
}

TEST(FileSystemTest, SetAttr) {
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);
  auto res = fs.alloc_inode(InodeType::FILE);
  ASSERT_TRUE(res.is_ok());
  auto inode = res.unwrap();

  auto first_free_block_num = fs.get_free_blocks_num().unwrap();
  std::cout << "first_free_block_num: " << first_free_block_num << std::endl;

  auto content = std::vector<u8>(KLargeFileMin);
  {
    auto res = fs.write_file(inode, content);
    ASSERT_TRUE(res.is_ok());
  }

  auto second_free_block_num = fs.get_free_blocks_num().unwrap();
  std::cout << "second_free_block_num: " << second_free_block_num << std::endl;

  content.resize(KLargeFileMax);
  {
    auto res = fs.write_file(inode, content);
    ASSERT_TRUE(res.is_ok());
  }

  auto third_free_block_num = fs.get_free_blocks_num().unwrap();
  ASSERT_TRUE(third_free_block_num < second_free_block_num);
  std::cout << "third_free_block_num: " << third_free_block_num << std::endl;

  { fs.resize(inode, KLargeFileMin).unwrap(); }
  auto final_free_block_num = fs.get_free_blocks_num().unwrap();
  ASSERT_EQ(final_free_block_num, second_free_block_num);
}

} // namespace chfs