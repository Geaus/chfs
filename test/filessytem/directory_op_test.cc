#include <random>

#include "./common.h"
#include "filesystem/directory_op.h"
#include "gtest/gtest.h"

namespace chfs {

TEST(FileSystemBase, Utilities) {
  std::vector<u8> content;
  std::list<DirectoryEntry> list;

  std::string input(content.begin(), content.end());

  parse_directory(input, list);
  ASSERT_TRUE(list.empty());

  input = append_to_directory(input, "test", 2);
  parse_directory(input, list);

  ASSERT_TRUE(list.size() == 1);

  for (uint i = 0; i < 100; i++) {
    input = append_to_directory(input, "test", i + 2);
  }
  list.clear();
  parse_directory(input, list);
  ASSERT_EQ(list.size(), 1 + 100);
}

TEST(FileSystemBase, UtilitiesRemove) {
  std::vector<u8> content;
  std::list<DirectoryEntry> list;

  std::string input(content.begin(), content.end());

  for (uint i = 0; i < 100; i++) {
    input = append_to_directory(input, "test" + std::to_string(i), i + 2);
  }

  input = rm_from_directory(input, "test0");
  // std::cout << input << std::endl;

  list.clear();
  parse_directory(input, list);
  ASSERT_EQ(list.size(), 99);

  input = rm_from_directory(input, "test12");
  list.clear();
  parse_directory(input, list);
  ASSERT_EQ(list.size(), 98);
}

TEST(FileSystemTest, DirectOperationAdd) {
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);

  auto res = fs.alloc_inode(InodeType::Directory);
  if (res.is_err()) {
    std::cerr << "Cannot allocate inode for root directory. " << std::endl;
    exit(1);
  }
  CHFS_ASSERT(res.unwrap() == 1, "The allocated inode number is incorrect ");

  std::list<DirectoryEntry> list;
  {
    auto res = read_directory(&fs, 1, list);
    ASSERT_TRUE(res.is_ok());
    ASSERT_TRUE(list.empty());
  }
}

TEST(FileSystemTest, mkdir) {
  auto bm =
      std::shared_ptr<BlockManager>(new BlockManager(kBlockNum, kBlockSize));
  auto fs = FileOperation(bm, kTestInodeNum);

  auto res = fs.alloc_inode(InodeType::Directory);
  if (res.is_err()) {
    std::cerr << "Cannot allocate inode for root directory. " << std::endl;
    exit(1);
  }

  for (uint i = 0; i < 100; i++) {
    auto res = fs.mkdir(1, ("test" + std::to_string(i)).c_str());
    ASSERT_TRUE(res.is_ok());
  }

  std::list<DirectoryEntry> list;
  {
    auto res = read_directory(&fs, 1, list);
    ASSERT_TRUE(res.is_ok());
  }
  ASSERT_EQ(list.size(), 100);
}

} // namespace chfs