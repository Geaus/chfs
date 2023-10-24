#include "common/util.h"
#include "gtest/gtest.h"

namespace chfs {

class UtilTest : public ::testing::Test {
protected:
  // This function is called before every test.
  void SetUp() override {}

  // This function is called after every test.
  void TearDown() override{};
};

TEST_F(UtilTest, SerializeAndDesrialize) {
  std::map<uint32_t, uint32_t> map;
  map[1] = 2;
  map[3] = 4;
  map[5] = 6;
  map[7] = 8;
  map[9] = 10;

  auto bytes = serialize_object(map);

  auto map_new = deserialize_object<std::map<uint32_t, uint32_t>>(bytes);

  EXPECT_EQ(map_new[1], 2);
}

TEST_F(UtilTest, WithEmptyMap) {
  std::map<uint32_t, uint32_t> map;

  auto bytes = serialize_object(map);

  auto map_new = deserialize_object<std::map<uint32_t, uint32_t>>(bytes);

  EXPECT_EQ(map_new.size(), 0);
}

} // namespace chfs