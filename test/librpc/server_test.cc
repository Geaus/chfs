#include "librpc/server.h"
#include "gtest/gtest.h"

namespace chfs {

class LibRpcServerTest : public ::testing::Test {
protected:
  const u16 port = 8080; // Default listening port
  std::shared_ptr<RpcServer> srv;
  // This function is called before every test.
  void SetUp() override { srv = std::make_shared<RpcServer>(port); }

  // This function is called after every test.
  void TearDown() override{};
};

TEST_F(LibRpcServerTest, Create) { EXPECT_EQ(srv.use_count(), 1); }

TEST_F(LibRpcServerTest, Bind) {
  srv->bind("add", [](double a, double b) { return a + b; });
  srv->bind("delete", [](double a, double b) { return a - b; });

  // Get names of all the bindings
  auto names = srv->names();
  std::vector<std::string> expected = {"delete", "add"};

  EXPECT_EQ(names, expected);
  EXPECT_EQ(names.size(), 2);
}

} // namespace chfs