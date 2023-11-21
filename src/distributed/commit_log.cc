#include <algorithm>

#include "common/bitmap.h"
#include "distributed/commit_log.h"
#include "distributed/metadata_server.h"
#include "filesystem/directory_op.h"
#include "metadata/inode.h"
#include <chrono>

namespace chfs {
/**
 * `CommitLog` part
 */
// {Your code here}
CommitLog::CommitLog(std::shared_ptr<BlockManager> bm,
                     bool is_checkpoint_enabled)
    : is_checkpoint_enabled_(is_checkpoint_enabled), bm_(bm) {

    std::vector<u8> buffer(bm_->block_size());
    auto bitmap = Bitmap(buffer.data(), bm->block_size());
    bitmap.zeroed();

    log_block_id = bm_->total_blocks() - 1024;
    for(block_id_t i = 0; i < kMaxLogSize+1 ; i++){
        bitmap.set(i);
    }

    bitmap_block_id = log_block_id+kMaxLogSize;
    bm_->write_block(bitmap_block_id,buffer.data());
    bm_->sync(bitmap_block_id);

}

CommitLog::~CommitLog() {}

// {Your code here}
auto CommitLog::get_log_entry_num() -> usize {
  // TODO: Implement this function.
    usize result = 0;
    for(block_id_t i = log_block_id; i < bitmap_block_id ; i++){

        std::vector<u8> buffer(bm_->block_size());
        bm_->read_block(i,buffer.data());
        auto * log= reinterpret_cast<Log_info *>(buffer.data());

        if(log->txn_id != 0) {
            result++;
        }
    }

    return result;
//  UNIMPLEMENTED();
  return 0;
}

// {Your code here}
auto CommitLog::append_log(txn_id_t txn_id,
                           std::vector<std::shared_ptr<BlockOperation>> ops)
    -> void {
  // TODO: Implement this function.

    std::vector<u8> bitmap_buffer(bm_->block_size());

    bm_->read_block(bitmap_block_id,bitmap_buffer.data());
    auto bitmap = Bitmap(bitmap_buffer.data(), bm_->block_size());

    for(block_id_t i = log_block_id; i < bitmap_block_id ; i++){

        std::vector<u8> buffer(bm_->block_size());
        bm_->read_block(i,buffer.data());
        auto * log= reinterpret_cast<Log_info *>(buffer.data());

        if(log->txn_id == 0){
            log->txn_id = txn_id;
            log->finished = false;
            usize index = 0;
            for(auto it = ops.begin(); it < ops.end(); it++){

                auto free_block = bitmap.find_first_free_w_bound(1024);
                if(free_block){

                    bitmap.set(free_block.value());
                   block_id_t free_block_id = free_block.value() + log_block_id;

                   log->block_id_map[index].first = (*it)->block_id_;
                   log->block_id_map[index].second = free_block_id;

                   bm_->write_block(free_block_id,(*it)->new_block_state_.data());

                   index++;
                }
            }
            bm_->write_block(i,buffer.data());
            break;
        }

    }

    bm_->write_block(bitmap_block_id,bitmap_buffer.data());

//  UNIMPLEMENTED();
}

// {Your code here}
auto CommitLog::commit_log(txn_id_t txn_id) -> void {
  // TODO: Implement this function.

    for(block_id_t i = log_block_id; i < bitmap_block_id ; i++){

        std::vector<u8> buffer(bm_->block_size());
        bm_->read_block(i,buffer.data());
        auto * log= reinterpret_cast<Log_info *>(buffer.data());

        if(log->txn_id == txn_id) {
            log->finished = true;
            bm_->write_block(i,buffer.data());
            break;
        }
    }
//  UNIMPLEMENTED();
}

// {Your code here}
auto CommitLog::checkpoint() -> void {
  // TODO: Implement this function.

  if(is_checkpoint_enabled_){
      std::vector<u8> bitmap_buffer(bm_->block_size());

      bm_->read_block(bitmap_block_id,bitmap_buffer.data());
      auto bitmap = Bitmap(bitmap_buffer.data(), bm_->block_size());

      for(block_id_t i = log_block_id; i < bitmap_block_id ; i++){

          std::vector<u8> buffer(bm_->block_size());
          bm_->read_block(i,buffer.data());
          auto * log= reinterpret_cast<Log_info *>(buffer.data());

          if(log->txn_id != 0 && log->finished){

              for(usize it = 0; it < log->npairs; it++){
                  block_id_t dst_id = log->block_id_map[it].first;
                  if(dst_id != 0){
                      block_id_t src_id = log->block_id_map[it].second;
                      std::vector<u8> back_up(bm_->block_size());
                      bm_->read_block(src_id,back_up.data());
                      bm_->write_block(dst_id,back_up.data());

                      bitmap.clear(src_id-log_block_id);
                  }
              }

              bm_->zero_block(i);
          }

      }

      bm_->write_block(bitmap_block_id,bitmap_buffer.data());
  }

//  UNIMPLEMENTED();
}

// {Your code here}
auto CommitLog::recover() -> void {
  // TODO: Implement this function.
    std::vector<u8> bitmap_buffer(bm_->block_size());

    bm_->read_block(bitmap_block_id,bitmap_buffer.data());
    auto bitmap = Bitmap(bitmap_buffer.data(), bm_->block_size());

    for(block_id_t i = log_block_id; i < bitmap_block_id ; i++){

        std::vector<u8> buffer(bm_->block_size());
        bm_->read_block(i,buffer.data());
        auto * log= reinterpret_cast<Log_info *>(buffer.data());

        if(log->txn_id != 0){

            for(usize it = 0; it < log->npairs; it++){
                block_id_t dst_id = log->block_id_map[it].first;
                if(dst_id != 0){
                    block_id_t src_id = log->block_id_map[it].second;
                    std::vector<u8> back_up(bm_->block_size());
                    bm_->read_block(src_id,back_up.data());
                    bm_->write_block(dst_id,back_up.data());

                }
            }
        }
    }
//  UNIMPLEMENTED();
}
}; // namespace chfs