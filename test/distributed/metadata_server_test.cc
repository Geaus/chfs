#include "distributed/dataserver.h"
#include "distributed/metadata_server.h"
#include "gtest/gtest.h"

namespace chfs {

class MetadataServerTest : public ::testing::Test {
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

  // And one client
  std::unique_ptr<RpcClient> cli;
  // This function is called before every test.
  void SetUp() override { init_sys(); };

  // This function is called after every test.
  void TearDown() override{};

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

TEST_F(MetadataServerTest, CreateDirThenLookup) {
  // This test create an directory at '/root/test_dir`
  auto dir_id = meta_srv->mknode(DirectoryType, 1, "test_dir");
  EXPECT_EQ(dir_id, 2);

  auto lookup_res = meta_srv->lookup(1, "test_dir");
  EXPECT_EQ(dir_id, lookup_res);
  // Don't forget to delete all files
  clean_data();
}

TEST_F(MetadataServerTest, WriteAnEmptyFile) {
  // First create an file at '/root/fileA'
  auto file_id = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id, 2);

  // Get the block map and it must be an empty one
  auto block_map = meta_srv->get_block_map(file_id);
  EXPECT_EQ(block_map.size(), 0);

  // Now the client realize that the file is empty
  // so it needs to allocate some blocks
  auto [block_id, machine_id, version] = meta_srv->allocate_block(file_id);
  EXPECT_GT(machine_id, 0);
  EXPECT_LT(machine_id, 4);
  // Now we have the block and we directly write some
  // bytes into the block
  // create a client to connect with the node
  auto cli = std::make_shared<RpcClient>("127.0.0.1",
                                         data_ports[machine_id - 1], true);

  // Write some bytes into the block
  std::vector<u8> bytes = {'H', 'e', 'l', 'l', 'o'};
  auto write_res = cli->call("write_data", block_id, 0, bytes);
  EXPECT_EQ(write_res.is_err(), false);
  EXPECT_EQ(write_res.unwrap()->as<bool>(), true);

  // Then check whether the data is persisted or not?
  auto read_res = cli->call("read_data", block_id, 0, 5, version);
  EXPECT_EQ(read_res.is_err(), false);
  auto read_bytes = read_res.unwrap()->as<std::vector<u8>>();
  EXPECT_EQ(bytes, read_bytes);

  // At last clean the data
  clean_data();
}

TEST_F(MetadataServerTest, CheckPersist) {
  // Create file and also write some data
  auto file_id = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id, 2);

  auto [block_id, machine_id, version] = meta_srv->allocate_block(file_id);
  EXPECT_GT(machine_id, 0);
  EXPECT_LT(machine_id, 4);

  auto cli = std::make_shared<RpcClient>("127.0.0.1",
                                         data_ports[machine_id - 1], true);

  std::vector<u8> bytes = {'H', 'e', 'l', 'l', 'o'};
  auto write_res = cli->call("write_data", block_id, 0, bytes);
  EXPECT_EQ(write_res.is_err(), false);
  EXPECT_EQ(write_res.unwrap()->as<bool>(), true);

  // Now let's just shutdown the whole fs
  cli.reset();
  shutdown();
  // And then reinit the whole system
  init_sys();

  // Then lookup the file and then read its content
  auto lookup_res = meta_srv->lookup(1, "fileA");
  EXPECT_EQ(lookup_res, 2);

  // Show me the block map
  auto block_map = meta_srv->get_block_map(file_id);
  EXPECT_EQ(block_map.size(), 1);
  auto [orig_block_id, orig_mac_id, orig_version] = block_map[0];
  EXPECT_EQ(orig_mac_id, machine_id);

  // Connect to the data server and read the data
  cli = std::make_shared<RpcClient>("127.0.0.1", data_ports[orig_mac_id - 1],
                                    true);

  auto read_res = cli->call("read_data", block_id, 0, 5, orig_version);
  EXPECT_EQ(read_res.is_err(), false);
  auto read_bytes = read_res.unwrap()->as<std::vector<u8>>();
  EXPECT_EQ(bytes, read_bytes);

  // Dont forget to clean data too
  clean_data();
}

TEST_F(MetadataServerTest, ReadWhenBlockIsInvalid) {
  // Create file and also write some data
  auto file_id = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id, 2);

  auto [block_id, machine_id, version] = meta_srv->allocate_block(file_id);
  EXPECT_GT(machine_id, 0);
  EXPECT_LT(machine_id, 4);

  auto cli = std::make_shared<RpcClient>("127.0.0.1",
                                         data_ports[machine_id - 1], true);

  std::vector<u8> bytes = {'H', 'e', 'l', 'l', 'o'};
  auto write_res = cli->call("write_data", block_id, 0, bytes);
  EXPECT_EQ(write_res.is_err(), false);
  EXPECT_EQ(write_res.unwrap()->as<bool>(), true);

  // And we free the block on the machine and reallocate one
  auto delete_res = cli->call("free_block", block_id);
  EXPECT_EQ(delete_res.is_err(), false);
  EXPECT_EQ(delete_res.unwrap()->as<bool>(), true);

  auto alloc_res = cli->call("alloc_block");
  EXPECT_EQ(alloc_res.is_err(), false);
  auto [new_block_id, new_version] =
      alloc_res.unwrap()->as<std::pair<block_id_t, version_t>>();
  EXPECT_EQ(new_block_id, block_id);
  EXPECT_EQ(new_version, version + 2); // one free it and one alloc it

  // Now the clients want to read the data with the original block version
  auto read_res = cli->call("read_data", block_id, 0, 5, version);
  EXPECT_EQ(read_res.is_err(), false);
  auto read_bytes = read_res.unwrap()->as<std::vector<u8>>();
  EXPECT_EQ(read_bytes.size(), 0);

  clean_data();
}

TEST_F(MetadataServerTest, CheckReadDir) {

  auto file_id1 = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id1, 2);

  auto file_id2 = meta_srv->mknode(RegularFileType, 1, "fileB");
  EXPECT_EQ(file_id2, 3);

  auto file_id3 = meta_srv->mknode(RegularFileType, 1, "fileC");
  EXPECT_EQ(file_id3, 4);

  auto dir_content = meta_srv->readdir(1);
  EXPECT_EQ(dir_content.size(), 3);
  EXPECT_EQ(dir_content[0].first, "fileA");
  EXPECT_EQ(dir_content[1].first, "fileB");
  EXPECT_EQ(dir_content[2].first, "fileC");

  EXPECT_EQ(dir_content[0].second, 2);
  EXPECT_EQ(dir_content[1].second, 3);
  EXPECT_EQ(dir_content[2].second, 4);

  clean_data();
}

TEST_F(MetadataServerTest, CheckUnlink) {
  auto dir_id = meta_srv->mknode(DirectoryType, 1, "test_dir");
  EXPECT_EQ(dir_id, 2);

  // unlink it from its parent
  auto unlink_res = meta_srv->unlink(1, "test_dir");
  EXPECT_EQ(unlink_res, true);

  // And check whether we can lookup it
  auto lookup_res = meta_srv->lookup(1, "test_dir");
  EXPECT_EQ(lookup_res, 0);

  // Recreate one!
  auto dir_id2 = meta_srv->mknode(DirectoryType, 1, "test_dir");
  EXPECT_EQ(dir_id2, 2);

  clean_data();
}

TEST_F(MetadataServerTest, CheckInvariant1) {
  auto file_id_1 = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id_1, 2);

  auto file_id_2 = meta_srv->mknode(RegularFileType, 1, "fileB");
  EXPECT_EQ(file_id_2, 3);

  bool is_started = false;
  std::vector<BlockInfo> t1_res, t2_res, t3_res, t4_res;
  std::thread t1([&is_started, &file_id_1, &t1_res, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_1);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      t1_res.push_back({block_id, machine_id, version});
    }
  });

  std::thread t2([&is_started, &file_id_1, &t2_res, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_1);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      t2_res.push_back({block_id, machine_id, version});
    }
  });

  std::thread t3([&is_started, &file_id_2, &t3_res, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_2);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      t3_res.push_back({block_id, machine_id, version});
    }
  });

  std::thread t4([&is_started, &file_id_2, &t4_res, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_2);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      t4_res.push_back({block_id, machine_id, version});
    }
  });

  is_started = true;
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  // Check the block map
  auto block_map_1 = meta_srv->get_block_map(file_id_1);
  EXPECT_EQ(block_map_1.size(), 200);

  auto block_map_2 = meta_srv->get_block_map(file_id_2);
  EXPECT_EQ(block_map_2.size(), 200);

  // Also check other invariants
  // First, check all the versions are equals to 1
  std::vector<std::set<block_id_t>> block_ids_in_mac;
  for (auto i = 0; i < 3; ++i) {
    block_ids_in_mac.push_back(std::set<block_id_t>());
  }
  for (auto i = 0; i < 100; ++i) {
    auto [block_id_t1, machine_id_t1, version_t1] = t1_res[i];
    auto [block_id_t2, machine_id_t2, version_t2] = t2_res[i];
    auto [block_id_t3, machine_id_t3, version_t3] = t3_res[i];
    auto [block_id_t4, machine_id_t4, version_t4] = t4_res[i];

    EXPECT_EQ(version_t1, 1);
    EXPECT_EQ(version_t2, 1);
    EXPECT_EQ(version_t3, 1);
    EXPECT_EQ(version_t4, 1);

    auto t1_res = block_ids_in_mac[machine_id_t1 - 1].insert(block_id_t1);
    EXPECT_EQ(t1_res.second, true);
    auto t2_res = block_ids_in_mac[machine_id_t2 - 1].insert(block_id_t2);
    EXPECT_EQ(t2_res.second, true);
    auto t3_res = block_ids_in_mac[machine_id_t3 - 1].insert(block_id_t3);
    EXPECT_EQ(t3_res.second, true);
    auto t4_res = block_ids_in_mac[machine_id_t4 - 1].insert(block_id_t4);
    EXPECT_EQ(t4_res.second, true);
  }

  // Check the block ids are unique
  auto block_num = 0;
  for (auto i = 0; i < 3; ++i) {
    block_num += block_ids_in_mac[i].size();
  }

  EXPECT_EQ(block_num, 400);

  clean_data();
}

TEST_F(MetadataServerTest, CheckInvariant2) {

  auto dir_id_1 = meta_srv->mknode(DirectoryType, 1, "SubDirA");
  EXPECT_EQ(dir_id_1, 2);
  auto dir_id_2 = meta_srv->mknode(DirectoryType, 1, "SubDirB");
  EXPECT_EQ(dir_id_2, 3);

  bool is_started = false;
  std::thread t1([&is_started, &dir_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file1_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_1, file_name);
      EXPECT_GT(file_id, 0);
    }
  });

  std::thread t2([&is_started, &dir_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file2_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_1, file_name);
      EXPECT_GT(file_id, 0);
    }
  });

  std::thread t3([&is_started, &dir_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file1_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_2, file_name);
      EXPECT_GT(file_id, 0);
    }
  });

  std::thread t4([&is_started, &dir_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file2_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_2, file_name);
      EXPECT_GT(file_id, 0);
    }
  });

  is_started = true;
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  // Check the content of these 2 directories
  auto dir_1_content = meta_srv->readdir(2);
  EXPECT_EQ(dir_1_content.size(), 200);

  auto dir_2_content = meta_srv->readdir(3);
  EXPECT_EQ(dir_2_content.size(), 200);

  // And there should not be any same inode ids
  std::set<inode_id_t> inode_ids;
  for (auto [name, inode_id] : dir_1_content) {
    auto res = inode_ids.insert(inode_id);
    EXPECT_EQ(res.second, true);
  }
  for (auto [name, inode_id] : dir_2_content) {
    auto res = inode_ids.insert(inode_id);
    EXPECT_EQ(res.second, true);
  }

  EXPECT_EQ(inode_ids.size(), 400);

  clean_data();
}

TEST_F(MetadataServerTest, CheckInvariant3) {
  // This time we try to allocate block and free block at the same time
  auto file_id_1 = meta_srv->mknode(RegularFileType, 1, "fileA");
  EXPECT_EQ(file_id_1, 2);

  auto file_id_2 = meta_srv->mknode(RegularFileType, 1, "fileB");
  EXPECT_EQ(file_id_2, 3);

  bool is_started = false;
  std::vector<BlockInfo> t1_res, t2_res, t3_res, t4_res;
  std::thread t1([&is_started, &file_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_1);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      auto del_res = meta_srv->free_block(file_id_1, block_id, machine_id);
      EXPECT_EQ(del_res, true);
    }
  });

  std::thread t2([&is_started, &file_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_1);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      auto del_res = meta_srv->free_block(file_id_1, block_id, machine_id);
      EXPECT_EQ(del_res, true);
    }
  });

  std::thread t3([&is_started, &file_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_2);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      auto del_res = meta_srv->free_block(file_id_2, block_id, machine_id);
      EXPECT_EQ(del_res, true);
    }
  });

  std::thread t4([&is_started, &file_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      auto [block_id, machine_id, version] =
          meta_srv->allocate_block(file_id_2);
      EXPECT_GT(machine_id, 0);
      EXPECT_LT(machine_id, 4);
      auto del_res = meta_srv->free_block(file_id_2, block_id, machine_id);
      EXPECT_EQ(del_res, true);
    }
  });

  is_started = true;
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  // Check the block map
  auto block_map_1 = meta_srv->get_block_map(file_id_1);
  EXPECT_EQ(block_map_1.size(), 0);

  auto block_map_2 = meta_srv->get_block_map(file_id_2);
  EXPECT_EQ(block_map_2.size(), 0);
  clean_data();
}

TEST_F(MetadataServerTest, CheckInvariant4) {
  // This test create and unlink files from these 2 directories at the same time
  auto dir_id_1 = meta_srv->mknode(DirectoryType, 1, "SubDirA");
  EXPECT_EQ(dir_id_1, 2);
  auto dir_id_2 = meta_srv->mknode(DirectoryType, 1, "SubDirB");
  EXPECT_EQ(dir_id_2, 3);

  // First creates 2 files as init state
  auto file_id1 = meta_srv->mknode(RegularFileType, dir_id_1, "fileA");
  EXPECT_EQ(file_id1, 4);
  auto file_id2 = meta_srv->mknode(RegularFileType, dir_id_2, "fileB");
  EXPECT_EQ(file_id2, 5);

  bool is_started = false;
  std::thread t1([&is_started, &dir_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file1_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_1, file_name);
      EXPECT_GT(file_id, 0);
      auto unlink_res = meta_srv->unlink(dir_id_1, file_name);
      EXPECT_EQ(unlink_res, true);
    }
  });

  std::thread t2([&is_started, &dir_id_1, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file2_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_1, file_name);
      EXPECT_GT(file_id, 0);
      auto unlink_res = meta_srv->unlink(dir_id_1, file_name);
      EXPECT_EQ(unlink_res, true);
    }
  });

  std::thread t3([&is_started, &dir_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file1_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_2, file_name);
      EXPECT_GT(file_id, 0);
      auto unlink_res = meta_srv->unlink(dir_id_2, file_name);
      EXPECT_EQ(unlink_res, true);
    }
  });

  std::thread t4([&is_started, &dir_id_2, this]() {
    while (!is_started) {
      ;
    }

    for (int i = 0; i < 100; i++) {
      std::string file_name = "file2_" + std::to_string(i);
      auto file_id = meta_srv->mknode(RegularFileType, dir_id_2, file_name);
      EXPECT_GT(file_id, 0);
      auto unlink_res = meta_srv->unlink(dir_id_2, file_name);
      EXPECT_EQ(unlink_res, true);
    }
  });

  is_started = true;
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  // Check the content of each directory
  auto dir_content_1 = meta_srv->readdir(dir_id_1);
  EXPECT_EQ(dir_content_1.size(), 1);
  EXPECT_EQ(dir_content_1[0].first, "fileA");
  EXPECT_EQ(dir_content_1[0].second, 4);

  auto dir_content_2 = meta_srv->readdir(dir_id_2);
  EXPECT_EQ(dir_content_2.size(), 1);
  EXPECT_EQ(dir_content_2[0].first, "fileB");
  EXPECT_EQ(dir_content_2[0].second, 5);

  clean_data();
}

} // namespace chfs