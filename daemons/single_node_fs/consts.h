//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// consts.h
// Note that during grading, we will overwrite this file
//
// Identification: daemons/single_node_fs/bitmap.h
//
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common/config.h"

/**
 * Constants used in the simple single-node filesystem
 *
 * These parameters can be tuned accordingly.
 * Nevertheless, we will use
 */
namespace chfs {

const usize kSingleMaxInode = 0;
const usize KBlockSize = 1024;
const u64 kDiskSize = 1024 * 1024 * 16;

const usize KMaxInodeNum = 1024;

} // namespace chfs