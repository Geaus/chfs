//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// util.h
//
// Identification: src/include/common/util.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/config.h"
#include "common/macros.h"
#include "rpc/msgpack.hpp"
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>

namespace chfs {

/**
 * A simple random number generator
 */
class RandomNumberGenerator {
public:
  RandomNumberGenerator() {
    std::random_device rd;
    seed.seed(rd());
  }

  auto rand(u32 min, u32 max) -> u32 {
    std::uniform_int_distribution<u32> dist(min, max);
    return dist(seed);
  }

private:
  std::mt19937_64 seed;
};

/**
 * Notice that these function might not be working on these
 * types not supported by `msgpack`.
 */
// Serialize an `object` into bytes
template <typename T>
auto serialize_object(T const &object) -> std::vector<u8> {
  auto buffer = std::make_shared<clmdep_msgpack::sbuffer>();
  clmdep_msgpack::pack(*buffer, object);

  std::vector<u8> bytes(buffer->data(), buffer->data() + buffer->size());
  return bytes;
}

// Deserialize bytes to an `object`
template <typename T>
auto deserialize_object(std::vector<u8> const &bytes) -> T {
  auto oh = clmdep_msgpack::unpack(reinterpret_cast<char const *>(bytes.data()),
                                   bytes.size());
  return oh.get().as<T>();
}

// Check whether a file exists or not
inline auto is_file_exist(std::string const &name) -> bool {
  std::ifstream file(name);
  return file.good();
}

/**
 * Helper function to dispatch a read/write requests into
 * many block read/write requests.
 *
 * @param offset: The offset of the file to read/write.
 * @param size: The size of the data to read/write.
 *
 * @return: A tuple of first, last block index and others for dispatching
 * block requests.
 */
inline auto dispatch_request(usize offset, usize size)
    -> std::tuple<usize, usize, usize, usize> {
  // First calculate the first and last block index
  auto first_block_index = ROUND_DOWN(offset, DiskBlockSize) / DiskBlockSize;
  auto last_block_index =
      ROUND_DOWN(offset + size, DiskBlockSize) / DiskBlockSize;

  auto first_block_offset = offset - first_block_index * DiskBlockSize;
  auto last_block_size = offset + size - last_block_index * DiskBlockSize;

  if (last_block_size == 0) {
    last_block_index -= 1;
    last_block_size = DiskBlockSize;
  }
  return std::make_tuple(first_block_index, last_block_index,
                         first_block_offset, last_block_size);
}

} // namespace chfs