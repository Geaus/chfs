#include <gtest/gtest.h>
#include <random>
#include <unordered_set>
#include <thread>
#include <chrono>

#include "distributed/metadata_server.h"
#include "distributed/dataserver.h"

namespace chfs {

class MetadataConcurrentStressTest : public ::testing::Test {
protected:
  // In this test, we simulate 4 nodes: 1 metadata server and 3 data servers.
  const u16 meta_port = 8080;
  const u16 data_ports[3] = {8081, 8082, 8083};

  const std::string inode_path = "/tmp/inode_file";
  const std::string data_path1 = "/tmp/test_file1";
  const std::string data_path2 = "/tmp/test_file2";
  const std::string data_path3 = "/tmp/test_file3";

  std::shared_ptr<MetadataServer> meta_srv;
  std::shared_ptr<DataServer> data_srv1;
  std::shared_ptr<DataServer> data_srv2;
  std::shared_ptr<DataServer> data_srv3;

  // This function is called before every test.
  void SetUp() override { init_sys(); };

  // This function is called after every test.
  void TearDown() override{ shutdown(); clean_data();};

  // Init the whole system
  void init_sys() {
    meta_srv = std::make_shared<MetadataServer>(meta_port, inode_path);
    data_srv1 = std::make_shared<DataServer>(data_ports[0], data_path1);
    data_srv2 = std::make_shared<DataServer>(data_ports[1], data_path2);
    data_srv3 = std::make_shared<DataServer>(data_ports[2], data_path3);

    meta_srv->reg_server("127.0.0.1", data_ports[0], true);
    meta_srv->reg_server("127.0.0.1", data_ports[1], true);
    meta_srv->reg_server("127.0.0.1", data_ports[2], true);
    meta_srv->run();
  }

  // Remove the current running system
  void shutdown() {
    meta_srv.reset();
    data_srv1.reset();
    data_srv2.reset();
    data_srv3.reset();
  }

  // Clean all files on the FS
  void clean_data() {
    std::remove(inode_path.c_str());
    std::remove(data_path1.c_str());
    std::remove(data_path2.c_str());
    std::remove(data_path3.c_str());
  };
};

TEST_F(MetadataConcurrentStressTest, StressTest1) {    
  const u64 test_iterations = 500;
  ASSERT_TRUE(test_iterations > 0);

  // Create two directories
  auto dir1_id = meta_srv->mknode(DirectoryType, 1, "dir1");
  ASSERT_EQ(dir1_id, 2);

  auto dir2_id = meta_srv->mknode(DirectoryType, 1, "dir2");
  ASSERT_EQ(dir2_id, 3);

  // Create two files base on these two directories
  auto file1_id = meta_srv->mknode(RegularFileType, dir1_id, "file1");
  ASSERT_EQ(file1_id, 4);
  auto file2_id = meta_srv->mknode(RegularFileType, dir2_id, "file2");
  ASSERT_EQ(file2_id, 5);

  // In this test, we run the test case for mknode/unlink and allocate block/free block of a node
  // in many threads and check the invariant we want.
  bool is_started = false;
  auto threads = std::vector<std::thread>();
  // Let's create 8 threads
  // First four try to allocate/unlink inodes from `dir1` and `dir2`
  for (u64 i = 0; i < 2; ++i) {
    auto thread_idx = i;
    threads.emplace_back([&, thread_idx]() {
    // Wait until all threads are started
    while (!is_started) {
      std::this_thread::yield();
    }

    auto inode_id_sets = std::unordered_set<inode_id_t>();
    for (u64 k = 0; k < test_iterations; ++k) {
      // Create 100 files in dir1
      for (u64 j = 0; j < 100; ++j) {
        std::string name = "file" + std::to_string(thread_idx) + '-' + std::to_string(j);
        auto file_id = meta_srv->mknode(RegularFileType, dir1_id, name);
        ASSERT_TRUE(file_id > 0);
        inode_id_sets.insert(file_id);
      }

      // Check invariants here
      EXPECT_EQ(inode_id_sets.size(), 100);

      // And delete 100 files in dir1
      for (u64 j = 0; j < 100; ++j) {
        std::string name = "file" + std::to_string(thread_idx) + '-' + std::to_string(j);
        auto del_res = meta_srv->unlink(dir1_id, name);
        ASSERT_TRUE(del_res);
      }

      inode_id_sets.clear();
    }
    });
  }

  for (u64 i = 0; i < 2; ++i) {
    auto thread_idx = i;
    threads.emplace_back([&, thread_idx]() {
    // Wait until all threads are started
    while (!is_started) {
      std::this_thread::yield();
    }

    auto inode_id_sets = std::unordered_set<inode_id_t>();
    for (u64 k = 0; k < test_iterations; ++k) {
      // Create 100 files in dir1
      for (u64 j = 0; j < 100; ++j) {
        std::string name = "file" + std::to_string(thread_idx) + '-' + std::to_string(j);
        auto file_id = meta_srv->mknode(RegularFileType, dir2_id, name);
        ASSERT_TRUE(file_id > 0);
        inode_id_sets.insert(file_id);
      }

      // Check invariants here
      EXPECT_EQ(inode_id_sets.size(), 100);

      // And delete 100 files in dir1
      for (u64 j = 0; j < 100; ++j) {
        std::string name = "file" + std::to_string(thread_idx) + '-' + std::to_string(j);
        auto del_res = meta_srv->unlink(dir2_id, name);
        ASSERT_TRUE(del_res);
      }

      inode_id_sets.clear();
    }
    });
  }

  // The next four threads allocate and deallocate blocks from `file1` and `file2`
  for (u64 i = 0; i < 2; ++i) {
    threads.emplace_back([&]() {
    // Wait until all threads are started
    while (!is_started) {
      std::this_thread::yield();
    }

    auto active_blocks = std::vector<std::pair<block_id_t, mac_id_t>>();
    for (u64 k = 0; k < test_iterations; ++k) {
      // Allocate 100 blocks
      for (u64 j = 0; j < 100; ++j) {
        auto [block_id, mac_id, version] = meta_srv->allocate_block(file1_id);
        ASSERT_TRUE(block_id > 0);
        ASSERT_TRUE(mac_id > 0);
        active_blocks.push_back({block_id, mac_id});
      }

      // Check invariants
      std::unordered_set<block_id_t> block_id_set_on_macs[3];  // 3 data servers
      for (auto [block_id, mac_id] : active_blocks) {
        block_id_set_on_macs[mac_id - 1].insert(block_id);
      }

      auto block_num = 0;
      for (auto l = 0; l < 3; ++l) {
        block_num += block_id_set_on_macs[l].size();
      }

      ASSERT_EQ(block_num, 100);

      // And free 100 blocks
      for (auto [block_id, mac_id] : active_blocks) {
        auto free_res = meta_srv->free_block(file1_id, block_id, mac_id);
        ASSERT_TRUE(free_res);
      }
      active_blocks.clear();
    }
    });
  }

  for (u64 i = 0; i < 2; ++i) {
    threads.emplace_back([&]() {
    // Wait until all threads are started
    while (!is_started) {
      std::this_thread::yield();
    }

    auto active_blocks = std::vector<std::pair<block_id_t, mac_id_t>>();
    for (u64 k = 0; k < test_iterations; ++k) {
      // Allocate 100 blocks
      for (u64 j = 0; j < 100; ++j) {
        auto [block_id, mac_id, version] = meta_srv->allocate_block(file2_id);
        ASSERT_TRUE(block_id > 0);
        ASSERT_TRUE(mac_id > 0);
        active_blocks.push_back({block_id, mac_id});
      }

      // Check invariants
      std::unordered_set<block_id_t> block_id_set_on_macs[3];  // 3 data servers
      for (auto [block_id, mac_id] : active_blocks) {
        block_id_set_on_macs[mac_id - 1].insert(block_id);
      }

      auto block_num = 0;
      for (auto l = 0; l < 3; ++l) {
        block_num += block_id_set_on_macs[l].size();
      }

      ASSERT_EQ(block_num, 100);

      // And free 100 blocks
      for (auto [block_id, mac_id] : active_blocks) {
        auto free_res = meta_srv->free_block(file2_id, block_id, mac_id);
        ASSERT_TRUE(free_res);
      }
      active_blocks.clear();
    }
    });
  }  

  // Record the start time
  auto start = std::chrono::high_resolution_clock::now();
  is_started = true;

  // Wait for all threads to finish
  for (auto &t : threads) {
    t.join();
  }

  // Record the end time
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;

  std::cout << "The test costs: " << duration.count() << " secs" << std::endl;

  // And check its invariant
  auto block_map_1 = meta_srv->get_block_map(file1_id);
  EXPECT_EQ(block_map_1.size(), 0);

  auto block_map_2 = meta_srv->get_block_map(file2_id);
  EXPECT_EQ(block_map_2.size(), 0);

auto dir_content_1 = meta_srv->readdir(dir1_id);
  EXPECT_EQ(dir_content_1.size(), 1);
  EXPECT_EQ(dir_content_1[0].first, "file1");
  EXPECT_EQ(dir_content_1[0].second, 4);

  auto dir_content_2 = meta_srv->readdir(dir2_id);
  EXPECT_EQ(dir_content_2.size(), 1);
  EXPECT_EQ(dir_content_2[0].first, "file2");
  EXPECT_EQ(dir_content_2[0].second, 5);
}

} // namespace chfs

int main(int argc, char **argv) {
  // Initialize Google Test
  ::testing::InitGoogleTest(&argc, argv);

  // Run the tests
  return RUN_ALL_TESTS();
}