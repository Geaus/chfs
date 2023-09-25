//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// superblock.h
//
// Identification: src/include/metadata/superblock.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "block/allocator.h"
#include "common/config.h"

namespace chfs {

typedef struct SuperBlockInternal {
  // Blocksize of the file system. It should be equal to the block size of the
  // block device.
  u32 block_size;
  // How many free blocks in the filesystem. The stored data may be stale on the
  // block size.
  u64 nblocks;
  // The maximum supported inode number.
  // it should match the `InodeManager.max_inode_supported`
  u64 ninodes;
  // The current filesystem size.
  u64 file_system_size;
} SuperblockInternal;

/**
 * Represent the super block of the filesystem
 * It records some critical information of the filesystem
 *
 * Note that by default, we assume the superblock will stored at the **first**
 * block of the underlying block device.
 *
 * It follows the inode filesystem described in the class
 */
class SuperBlock {
  std::shared_ptr<BlockManager> bm;
  SuperBlockInternal inner;

public:
  /**
   * Constructor
   * Note that this constructor is called upon the first time the filesystem is
   * created.
   *
   * @param bm the block manager
   * @param ninodes the number of inodes
   *
   */
  SuperBlock(std::shared_ptr<BlockManager> bm, u64 ninodes);

  /**
   * Create a superblock from a block manager,
   * assuming the super block has been initialized
   *
   * @param bm the block manager
   * @param id the block id of the super block
   */
  static auto create_from_existing(std::shared_ptr<BlockManager> bm,
                                   block_id_t id)
      -> ChfsResult<std::shared_ptr<SuperBlock>>;

  auto flush(block_id_t id) const -> ChfsNullResult {
    return bm->write_partial_block(id, (u8 *)&inner, 0,
                                   sizeof(SuperBlockInternal));
  }

  /**
   * Getters
   */
  u64 get_file_system_size() const { return inner.file_system_size; }
  u32 get_block_size() const { return inner.block_size; }
  u64 get_nblocks() const { return inner.nblocks; }
  u64 get_ninodes() const { return inner.ninodes; }

private:
  explicit SuperBlock(std::shared_ptr<BlockManager> bm) : bm(bm) {}
};

} // namespace chfs