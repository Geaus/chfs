#include "distributed/client.h"
#include "distributed/dataserver.h"
#include "distributed/metadata_server.h"
#include "gtest/gtest.h"

namespace chfs {

class DistributedClientTest : public ::testing::Test {
protected:
  const u16 meta_port = 8080;
  const u16 data_ports[3] = {8081, 8082, 8083};

  const std::string inode_path = "/tmp/inode_file";
  const std::string data_path1 = "/tmp/test_file1";
  const std::string data_path2 = "/tmp/test_file2";
  const std::string data_path3 = "/tmp/test_file3";

  std::shared_ptr<ChfsClient> client;
  std::shared_ptr<MetadataServer> meta_srv;
  std::shared_ptr<DataServer> data_srv1;
  std::shared_ptr<DataServer> data_srv2;
  std::shared_ptr<DataServer> data_srv3;

  // This function is called before every test.
  void SetUp() override { init_sys(); }

  // This function is called after every test.
  void TearDown() override {
    shutdown();
    clean_data(); // We don't test persistence here since it's already tested.
  };

  void init_sys() {
    meta_srv = std::make_shared<MetadataServer>(meta_port, inode_path);
    data_srv1 = std::make_shared<DataServer>(data_ports[0], data_path1);
    data_srv2 = std::make_shared<DataServer>(data_ports[1], data_path2);
    data_srv3 = std::make_shared<DataServer>(data_ports[2], data_path3);
    client = std::make_shared<ChfsClient>();

    meta_srv->reg_server("127.0.0.1", data_ports[0], true);
    meta_srv->reg_server("127.0.0.1", data_ports[1], true);
    meta_srv->reg_server("127.0.0.1", data_ports[2], true);
    meta_srv->run();

    client->reg_server(ChfsClient::ServerType::METADATA_SERVER, "127.0.0.1",
                       meta_port, true);
    client->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                       data_ports[0], true);
    client->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                       data_ports[1], true);
    client->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                       data_ports[2], true);
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

TEST_F(DistributedClientTest, WriteAndThenRead) {
  auto inode = client->mknode(ChfsClient::FileType::REGULAR, 1, "test_file");
  EXPECT_EQ(inode.is_ok(), true);
  auto inode_id = inode.unwrap();
  ASSERT_EQ(inode_id, 2);

  std::vector<u8> data = {'H', 'e', 'l', 'l', 'o'};
  // We choose to write 5 bytes between block 1 and block 2.
  auto file_offset = DiskBlockSize - 1;
  auto write_res = client->write_file(inode_id, file_offset, data);
  EXPECT_EQ(write_res.is_ok(), true);

  // Then read these bytes
  auto read_res = client->read_file(inode_id, file_offset, data.size());
  EXPECT_EQ(read_res.is_ok(), true);
  EXPECT_EQ(read_res.unwrap(), data);
}

TEST_F(DistributedClientTest, CreateConcurrent) {
  auto client2 = std::make_shared<ChfsClient>();
  client2->reg_server(ChfsClient::ServerType::METADATA_SERVER, "127.0.0.1",
                      meta_port, true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[0], true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[1], true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[2], true);

  bool start = false;
  ChfsResult<inode_id_t> t1_res = ErrorType::INVALID;
  ChfsResult<inode_id_t> t2_res = ErrorType::INVALID;
  std::thread t1([&t1_res, &start, this]() {
    while (!start)
      ;
    t1_res = client->mknode(ChfsClient::FileType::REGULAR, 1, "test_file");
  });

  std::thread t2([&t2_res, &start, &client2]() {
    while (!start)
      ;
    t2_res = client2->mknode(ChfsClient::FileType::REGULAR, 1, "test_file");
  });

  start = true;
  t1.join();
  t2.join();
  // One should be regarded as success and the other one is failed
  if (t1_res.is_ok()) {
    EXPECT_EQ(t1_res.unwrap(), 2);
  } else {
    EXPECT_EQ(t2_res.unwrap(), 2);
  }
}

TEST_F(DistributedClientTest, ReCreateWhenDelete) {
  // Create an inode first
  auto inode = client->mknode(ChfsClient::FileType::REGULAR, 1, "test_file");
  EXPECT_EQ(inode.is_ok(), true);
  auto inode_id = inode.unwrap();
  EXPECT_EQ(inode_id, 2);

  auto client2 = std::make_shared<ChfsClient>();
  client2->reg_server(ChfsClient::ServerType::METADATA_SERVER, "127.0.0.1",
                      meta_port, true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[0], true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[1], true);
  client2->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
                      data_ports[2], true);

  bool start = false;
  ChfsResult<inode_id_t> t1_res = ErrorType::INVALID;
  ChfsNullResult t2_res = ErrorType::INVALID;
  std::thread t1([&t1_res, &start, this]() {
    while (!start)
      ;
    t1_res = client->mknode(ChfsClient::FileType::REGULAR, 1, "test_file");
  });

  std::thread t2([&t2_res, &start, &client2]() {
    while (!start)
      ;
    t2_res = client2->unlink(1, "test_file");
  });

  start = true;
  t1.join();
  t2.join();
  // One should be regarded as success and the other one is failed
  if (t1_res.is_ok()) {
    EXPECT_EQ(t1_res.unwrap(), 2);
  } else {
    EXPECT_EQ(t2_res.is_ok(), true);
  }
}

} // namespace chfs