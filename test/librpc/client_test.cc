#include "common/result.h"
#include "librpc/client.h"
#include "librpc/server.h"
#include "gtest/gtest.h"
#include <ctime>
#include <unistd.h>
#include <unordered_map>

namespace chfs {

class LibRpcClientTest : public ::testing::Test {
protected:
  const u16 port = 8080; // Default listening port
  std::shared_ptr<RpcServer> srv;

  // This function is called before every test.
  void SetUp() override { srv = std::make_shared<RpcServer>(port); }

  // This function is called after every test.
  void TearDown() override{};
};

TEST_F(LibRpcClientTest, CreateAndConnect) {
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);
  // Wait for some moment to make the connection established
  const timespec time = {
      0,
      500 * 1000L * 1000L,
  };
  nanosleep(&time, nullptr);

  // Check the status of the connection
  EXPECT_EQ(cli->get_connection_state(),
            rpc::client::connection_state::connected);
}

TEST_F(LibRpcClientTest, BindAndCallSimple) {
  srv->bind("add", [](int a, int b) { return a + b; });
  srv->bind("delete", [](int a, int b) { return a - b; });

  // Start the server for accepting rpc requests
  srv->run(true, 2);

  // Launch a client & sending requests
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);

  auto add_res = cli->call("add", 2, 3);
  EXPECT_EQ(add_res.is_ok(), true);
  EXPECT_EQ(add_res.unwrap()->as<int>(), 5);

  auto del_res = cli->call("delete", 8, 3);
  EXPECT_EQ(del_res.is_ok(), true);
  EXPECT_EQ(del_res.unwrap()->as<int>(), 5);
}

TEST_F(LibRpcClientTest, BindAndCallAsync) {
  srv->bind("add", [](int a, int b) {
    sleep(1);
    return a + b;
  });

  // Start the server for accepting rpc requests
  srv->run(true, 2);

  // Launch a client & sending requests
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);

  auto add_future = cli->async_call("add", 2, 3).unwrap();
  // Check immediately
  auto ft_state = add_future->wait_for(std::chrono::seconds(0));
  EXPECT_EQ(ft_state, std::future_status::timeout);

  // Wait for some moment to make the async call finished
  sleep(2);

  ft_state = add_future->wait_for(std::chrono::seconds(0));
  EXPECT_EQ(ft_state, std::future_status::ready);

  EXPECT_EQ(add_future->get().as<int>(), 5);
}

TEST_F(LibRpcClientTest, CallGlobalVariable) {
  int cnt = 0;

  srv->bind("add", [&cnt](int a, int b) {
    cnt += a + b;
    return a + b;
  });

  // Start the server for accepting rpc requests
  srv->run(true, 2);

  // Launch a client & sending requests
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);

  for (int i = 0; i < 5; ++i) {
    auto add_res = cli->call("add", 2, 3);
    EXPECT_EQ(add_res.is_err(), false);
  }

  EXPECT_EQ(cnt, 25);
}

TEST_F(LibRpcClientTest, WithNetworkFailure) {
  srv->bind("add", [](int a, int b) { return a + b; });

  // Start the server for accepting rpc requests
  srv->run(true, 2);

  // Launch a client & sending requests
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, false);
  int failure_cnt = 0;

  for (int i = 0; i < 100; ++i) {
    auto res = cli->call("add", 2, 3);
    if (res.is_err())
      failure_cnt += 1;
  }

  EXPECT_GE(failure_cnt, 0);

  // Wait for the memory collection to prevent the false `memory leak`
  // problem reported by `LeakSanitizer`.
  sleep(10);
}

TEST_F(LibRpcClientTest, RpcSemantic) {
  int cnt = 0;

  srv->bind("add", [&cnt](int a, int b) {
    cnt += a + b;
    return a + b;
  });

  // Start the server for accepting rpc requests
  srv->run(true, 2);

  // Launch a client & sending requests
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, false);
  int failure_cnt = 0;

  for (int i = 0; i < 100; ++i) {
    auto res = cli->call("add", 2, 3);
    if (res.is_err())
      failure_cnt += 1;
  }

  EXPECT_GE(failure_cnt, 0);
  EXPECT_GE(cnt, 0);

  sleep(10);
}

TEST_F(LibRpcClientTest, DeliverBytes) {
  // The test is to show how to deliver byte codes of librpc

  // Allocate some bytes and assume them like blocks
  auto vblocks = new char[1024];

  // Init blocks
  for (int i = 0; i < 1024; ++i)
    vblocks[i] = '0' + i % 10;

  /*
   * We give the server a function that receives a buffer
   * and write some bytes into the buffer. And a function
   * read some bytes from `vblocks`.
   */
  srv->bind("read_bytes", [&vblocks](uint32_t offset, uint32_t length) {
    const char *data = vblocks + offset;
    std::vector<char> buffer(data, data + length);

    return buffer;
  });

  srv->bind("write_bytes",
            [&vblocks](const std::vector<char> &buffer, uint32_t offset) {
              char *data = vblocks + offset;
              memcpy(data, buffer.data(), buffer.size());
            });

  // Now we can test whether the rpc works perfectly or not
  srv->run(true, 2);

  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);
  // First, read some bytes from server
  auto read_res = cli->call("read_bytes", 10, 5);
  EXPECT_EQ(read_res.is_ok(), true);

  auto res_buffer = read_res.unwrap()->as<std::vector<char>>();
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(res_buffer[i], '0' + i);

  // Let's write some content to modify the data in server's block
  std::vector<char> new_buffer = {'H', 'e', 'l', 'l', 'o'};
  auto write_res = cli->call("write_bytes", new_buffer, 10);
  EXPECT_EQ(write_res.is_ok(), true);
  // And read again
  read_res = cli->call("read_bytes", 10, 5);
  EXPECT_EQ(read_res.is_ok(), true);

  res_buffer = read_res.unwrap()->as<std::vector<char>>();
  EXPECT_EQ(res_buffer, new_buffer);

  // Don't forget to free memory
  delete[] vblocks;
}

TEST_F(LibRpcClientTest, DeliverMap) {
  // Init a map and insert some elements
  std::unordered_map<uint64_t, uint32_t> umap;
  for (int i = 0; i < 5; ++i)
    umap[i] = '0' + i;

  /*
   * We give the server a function that receives a buffer
   * and write some bytes into the buffer. And a function
   * read some bytes from `vblocks`.
   */
  srv->bind("read", [&umap]() { return umap; });

  // Now we can test whether the rpc works perfectly or not
  srv->run(true, 2);

  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);
  // First, read some bytes from server
  auto read_res = cli->call("read");
  EXPECT_EQ(read_res.is_ok(), true);

  auto res_map =
      read_res.unwrap()->as<std::unordered_map<uint64_t, uint32_t>>();
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(res_map[i], '0' + i);
}

} // namespace chfs