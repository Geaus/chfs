//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// common.h
//
// Common test configurations of the lab
//
// Identification: test/filessytem/common.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/config.h"

namespace chfs {

// some sample test parameters
const usize kFileNum = 50;
const usize kTestInodeNum = 1024 * 8;
const usize kDiskSize = 16 * 1024 * 1024;
const usize kBlockSize = 512;
const usize kBlockNum = kDiskSize / kBlockSize;

const usize KLargeFileMin = 512 * 10;
const usize KLargeFileMax = 512 * 100;

inline auto get_test_seed() -> u64 { return 0xdeadbeaf; }

} // namespace chfs