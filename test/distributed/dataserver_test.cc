#include "distributed/dataserver.h"
#include "librpc/client.h"
#include "gtest/gtest.h"
#include <fstream>

namespace chfs {

class DataServerTest : public ::testing::Test {
protected:
  const std::string data_path = "/tmp/test_file";
  std::shared_ptr<DataServer> data_srv;
  const u16 port = 8080;

  // This function is called before every test.
  void SetUp() override {
    data_srv = std::make_shared<DataServer>(port, data_path);
  }

  // This function is called after every test.
  void TearDown() override {
    // Don't forget to remove the file to save memory
    std::remove(data_path.c_str());
  };
};

TEST_F(DataServerTest, CreateAndInit) {
  // Check whether the file exists or not
  std::ifstream file(data_path);
  EXPECT_EQ(file.good(), true);
}

TEST_F(DataServerTest, AllocateAndDelete) {
  // Create a RPC client and connect with the data server
  auto cli = std::make_unique<RpcClient>("127.0.0.1", 8080, true);

  // Try to allocate a block
  auto res = cli->call("alloc_block");
  EXPECT_EQ(res.is_err(), false);
  auto [block_id, version] =
      res.unwrap()->as<std::pair<block_id_t, version_t>>();
  EXPECT_GT(block_id, 0); // The first one should be version map
  EXPECT_EQ(version, 1);

  // Then delete this one and reallocate
  res = cli->call("free_block", block_id);
  EXPECT_EQ(res.is_ok(), true);
  EXPECT_EQ(res.unwrap()->as<bool>(), true);

  res = cli->call("alloc_block");
  EXPECT_EQ(res.is_err(), false);
  auto [realloc_id, new_version] =
      res.unwrap()->as<std::pair<block_id_t, version_t>>();
  EXPECT_EQ(block_id, realloc_id);
  EXPECT_EQ(new_version, 3);
}

TEST_F(DataServerTest, ReadAndWrite) {
  // Create a RPC client and connect with the data server
  auto cli = std::make_unique<RpcClient>("127.0.0.1", 8080, true);

  // Try to allocate a block
  auto res = cli->call("alloc_block");
  EXPECT_EQ(res.is_err(), false);
  auto [block_id, version] =
      res.unwrap()->as<std::pair<block_id_t, version_t>>();
  EXPECT_GT(block_id, 0);

  // At first read 8 bytes somewhere inner the block
  // It should be all zero
  res = cli->call("read_data", block_id, 24, 8, version);
  EXPECT_EQ(res.is_err(), false);
  auto buffer = res.unwrap()->as<std::vector<u8>>();
  EXPECT_EQ(buffer.size(), 8);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(buffer[i], 0);
  }

  // Write some contents in the same pos
  std::vector<u8> input = {'H', 'e', 'l', 'l', 'o'};
  res = cli->call("write_data", block_id, 24, input);
  EXPECT_EQ(res.is_err(), false);
  EXPECT_EQ(res.unwrap()->as<bool>(), true);

  res = cli->call("read_data", block_id, 24, 5, version);
  EXPECT_EQ(res.is_ok(), true);
  buffer = res.unwrap()->as<std::vector<u8>>();
  EXPECT_EQ(buffer, input);
}

} // namespace chfs