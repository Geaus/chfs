#include "distributed/commit_log.h"
#include "distributed/metadata_server.h"
#include "gtest/gtest.h"
#include <chrono>
#include <thread>

namespace chfs {

class CommitLogTest : public ::testing::Test {
protected:
  const u16 meta_port = 8080;
  const std::string inode_path = "/tmp/inode_file";

  std::shared_ptr<MetadataServer> meta_srv;
  // This function is called before every test.
  void SetUp() override{};

  // This function is called after every test.
  void TearDown() override{};
};

TEST_F(CommitLogTest, CheckConcurrent) {
  auto meta_srv = std::make_shared<MetadataServer>(meta_port, inode_path, true);

  bool is_started = false;
  std::thread t1([&]() {
    while (!is_started) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto mk_res = meta_srv->mknode(DirectoryType, 1, "dir");
    EXPECT_GT(mk_res, 1);
  });

  std::thread t2([&]() {
    while (!is_started) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto mk_res = meta_srv->mknode(DirectoryType, 1, "dir-2");
    EXPECT_GT(mk_res, 1);
  });

  std::cerr << "Start to test\n\n\n";

  is_started = true;
  t1.join();
  t2.join();

  auto dir_id1 = meta_srv->lookup(1, "dir");
  EXPECT_GT(dir_id1, 1);

  auto dir_id2 = meta_srv->lookup(1, "dir-2");
  EXPECT_GT(dir_id2, 1);

  auto dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 2);

  meta_srv->recover();
  // Check the result
  dir_id1 = meta_srv->lookup(1, "dir");
  EXPECT_GT(dir_id1, 1);

  dir_id2 = meta_srv->lookup(1, "dir-2");
  EXPECT_GT(dir_id2, 1);

  dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 2);

  std::remove(inode_path.c_str());
}

TEST_F(CommitLogTest, CheckConcurrentUnlink) {
  auto meta_srv = std::make_shared<MetadataServer>(meta_port, inode_path, true);

  auto mk_res = meta_srv->mknode(DirectoryType, 1, "dir");
  EXPECT_EQ(mk_res, 2);

  mk_res = meta_srv->mknode(DirectoryType, 1, "dir-2");
  EXPECT_EQ(mk_res, 3);

  bool is_started = false;
  std::thread t1([&]() {
    while (!is_started) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto del_res = meta_srv->unlink(1, "dir");
    EXPECT_EQ(del_res, true);
  });

  std::thread t2([&]() {
    while (!is_started) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto del_res = meta_srv->unlink(1, "dir-2");
    EXPECT_EQ(del_res, true);
  });

  is_started = true;
  t1.join();
  t2.join();

  auto dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 0);

  meta_srv->recover();
  dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 0);

  std::remove(inode_path.c_str());
}

TEST_F(CommitLogTest, CheckRecoverFromFailure) {
  auto meta_srv =
      std::make_shared<MetadataServer>(meta_port, inode_path, true, true, true);

  auto mk_res = meta_srv->mknode(DirectoryType, 1, "dir");
  EXPECT_EQ(mk_res, 0);

  // error occurs
  meta_srv->recover();

  auto dir_id1 = meta_srv->lookup(1, "dir");
  EXPECT_NE(dir_id1, 0);

  auto dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 1);
  EXPECT_EQ(dir_content[0].first, "dir");
  EXPECT_EQ(dir_content[0].second, dir_id1);

  std::remove(inode_path.c_str());
}

TEST_F(CommitLogTest, CheckCheckpointFunctional) {
  auto meta_srv =
      std::make_shared<MetadataServer>(meta_port, inode_path, true, true);

  for (int i = 0; i < 100; i++) {
    auto mk_res =
        meta_srv->mknode(DirectoryType, 1, "dir-" + std::to_string(i));
    EXPECT_GT(mk_res, 1);
  }

  std::cerr << "mknode done\n";

  for (int i = 0; i < 100; i++) {
    auto del_res = meta_srv->unlink(1, "dir-" + std::to_string(i));
    EXPECT_EQ(del_res, true);
  }

  std::cerr << "unlink done\n";

  // Check log entries
  auto log_entries = meta_srv->get_log_entries();
  EXPECT_LT(log_entries, 200);

  std::cerr << "check root dir\n";

  meta_srv->recover();
  // Check the system's status
  auto dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 0);

  std::remove(inode_path.c_str());
}

} // namespace chfs