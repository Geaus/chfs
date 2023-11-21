//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// commit_log.h
//
// Identification: src/include/distributed/commit_log.h
//
//
//===----------------------------------------------------------------------===//
#pragma once

#include "block/manager.h"
#include "common/config.h"
#include "common/macros.h"
#include "filesystem/operations.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace chfs {
/**
 * `BlockOperation` is an entry indicates an old block state and
 * a new block state. It's used to redo the operation when
 * the system is crashed.
 */
class BlockOperation {
public:
  explicit BlockOperation(block_id_t block_id, std::vector<u8> new_block_state)
      : block_id_(block_id), new_block_state_(new_block_state) {
    CHFS_ASSERT(new_block_state.size() == DiskBlockSize, "invalid block state");
  }

  block_id_t block_id_;
  std::vector<u8> new_block_state_;
};

/**
 * `CommitLog` is a class that records the block edits into the
 * commit log. It's used to redo the operation when the system
 * is crashed.
 */
class CommitLog {
public:
  explicit CommitLog(std::shared_ptr<BlockManager> bm,
                     bool is_checkpoint_enabled);
  ~CommitLog();
  auto append_log(txn_id_t txn_id,
                  std::vector<std::shared_ptr<BlockOperation>> ops) -> void;
  auto commit_log(txn_id_t txn_id) -> void;
  auto checkpoint() -> void;
  auto recover() -> void;
  auto get_log_entry_num() -> usize;

  bool is_checkpoint_enabled_;
  std::shared_ptr<BlockManager> bm_;
  /**
   * {Append anything if you need}
   */
  block_id_t log_block_id;
  block_id_t bitmap_block_id;
};

class Log_info{

public:

    txn_id_t txn_id ;
    bool finished ;
    u32 npairs;

    [[maybe_unused]] std::pair<block_id_t,block_id_t> block_id_map[0];

public:

    Log_info(txn_id_t id = 0, bool finish = false)
    :txn_id(id), finished(finish) {
        npairs = (DiskBlockSize - sizeof(Log_info)) / (sizeof (block_id_t) * 2);
    }

    auto flush_to_buffer(u8 *buffer) const {

        memcpy(buffer, this, sizeof(Log_info));
        auto log_info_p = reinterpret_cast<Log_info *>(buffer);

        for(usize i = 0; i < kMaxLogSize; i++){
            std::get<0>(log_info_p->block_id_map[i]) = 0;
            std::get<1>(log_info_p->block_id_map[i]) = 0;
        }

    }
};
} // namespace chfs