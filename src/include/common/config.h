#pragma once

#include <cstdint>

namespace chfs {

using u8 = uint8_t;
using i8 = int8_t;
using i32 = int32_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using usize = unsigned int;

using block_id_t = u64; // page id type
using inode_id_t = u64;
using mac_id_t = u32;
using txn_id_t = u32;
using version_t = u32;

const usize KDefaultBlockCnt = 4096; // use a default 8MB file size
const usize DiskBlockSize = 4096;    // 4KB
const usize DistributedMaxInodeSupported = 4096;
const usize kMaxLogBlockSize = 10 * 1024; // 40MB, 10 * 1K * 4K/per block = 40M
const usize kMaxLogSize = 128; // when this reaches, trigger checkpoint

} // namespace chfs
