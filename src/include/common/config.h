#pragma once

#include <cstdint>

namespace chfs {

using u8 = uint8_t;
using i8 = int8_t;
using i32 = int32_t;
using u32 = uint32_t;
using u64 = uint64_t;

using usize = unsigned int;

using block_id_t = u64; // page id type
using inode_id_t = u64;

const usize KDefaultBlockCnt = 4096; // use a default 8MB file size

} // namespace chfs
