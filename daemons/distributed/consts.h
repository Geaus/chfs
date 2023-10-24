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
#include <string>

/**
 * Constants used in the distributed filesystem
 */
namespace chfs {

const u16 kMetadataServerPort = 8080;
const usize kDataServerNum = 3;
const u16 kDataServerPorts[] = {8081, 8082, 8083};

const std::string kMetaBlockPath = "/tmp/meta_block.bin";
const std::string kDataBlockPath[] = {"/tmp/data_block_1.bin", "/tmp/data_block_2.bin", "/tmp/data_block_3.bin"};

} // namespace chfs