#include <string.h>

#include "metadata/superblock.h"

namespace chfs {

SuperBlock::SuperBlock(std::shared_ptr<BlockManager> bm, u64 ninodes) : bm(bm) {
  this->inner.block_size = bm->block_size();
  this->inner.nblocks = bm->total_blocks();
  this->inner.ninodes = ninodes;

  CHFS_VERIFY(this->inner.block_size >= sizeof(SuperBlockInternal),
              "Block size too small");
}

auto SuperBlock::create_from_existing(std::shared_ptr<BlockManager> bm,
                                      block_id_t id)
    -> ChfsResult<std::shared_ptr<SuperBlock>> {
  auto res = std::shared_ptr<SuperBlock>(new SuperBlock(bm));
  std::vector<u8> buffer(bm->block_size());
  auto read_res = bm->read_block(id, buffer.data());
  if (read_res.is_err()) {
    return ChfsResult<std::shared_ptr<SuperBlock>>(read_res.unwrap_error());
  }
  memcpy(&res->inner, buffer.data(), sizeof(SuperBlockInternal));
  return ChfsResult<std::shared_ptr<SuperBlock>>(res);
}

} // namespace chfs