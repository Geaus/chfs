#pragma once

#include "common/macros.h"
#include "block/manager.h"
#include <mutex>
#include <vector>
#include <cstring>

namespace chfs {
template<typename Command>
class log_entry {
public:
    int index;
    int term;
    Command cmd;

    log_entry(int index = 0, int term = 0) : index(index), term(term) {}
    log_entry(int index, int term, Command cmd) : index(index), term(term), cmd(cmd) {}
};
class Entry{
public:
    int index;
    int term;
    int value;
    int size;
};
/** 
 * RaftLog uses a BlockManager to manage the data..
 */
template <typename Command>
class RaftLog {
public:
    RaftLog(std::shared_ptr<BlockManager> bm);
    ~RaftLog();

    /* Lab3: Your code here */
    void restore(int &current_term, int &votedFor, std::vector<log_entry<Command>> &log, std::vector<u8> &snapshot);
    void persist_meta(int currentTerm, int votedFor);
    void persist_log(std::vector<log_entry<Command>> log);
    void append_log(log_entry<Command>& entry);
    void append_logs(std::vector<log_entry<Command>>& log);
    void persist_snapshot(std::vector<u8> &snapshot);


private:
    std::shared_ptr<BlockManager> bm_;
    std::mutex mtx;
    /* Lab3: Your code here */

};

template <typename Command>
RaftLog<Command>::RaftLog(std::shared_ptr<BlockManager> bm)
{
    /* Lab3: Your code here */
    this->bm_ = bm;
}

template <typename Command>
RaftLog<Command>::~RaftLog()
{
    /* Lab3: Your code here */
    bm_.reset();
}
// 0 if persist log
// 1 currentTerm
// 2 votedFor
// 3 log_block_num
// 4 log size
// 5 if persist snapshot
// 6 snapshot size
/* Lab3: Your code here */
template <typename Command>
void RaftLog<Command>::restore(int &current_term, int &votedFor, std::vector<log_entry<Command>> &log, std::vector<u8> &snapshot) {
    std::unique_lock<std::mutex> lock(mtx);

    std::vector<u8> meta_buffer(bm_->block_size());
    std::vector<u8> log_buffer(bm_->block_size());
    std::vector<u8> snap_buffer(bm_->block_size());
    int entry_per_block = bm_->block_size() / sizeof(Entry);
    const int snap_block_id = 17;
//    snapshot.clear();
    bm_->read_block(0,meta_buffer.data());
    int *meta_p = reinterpret_cast<int *>(meta_buffer.data());

    if(meta_p[0] == 0){
        current_term = 0;
        votedFor = -1;
        log_entry<Command> first_empty_log = log_entry<Command>();
        log.push_back(first_empty_log);

        meta_p[1] = 0;
        meta_p[2] = -1;
        meta_p[3] = 1;
        meta_p[4] = 1;
        bm_->write_block(0,meta_buffer.data());
        bm_->sync(0);
        return ;
    }
    else if (meta_p[0] == 1){

        current_term = meta_p[1];
        votedFor = meta_p[2];
        int total_block_num = meta_p[3];
        int log_size = meta_p[4];

//        log_entry<Command> first_empty_log = log_entry<Command>();
//        log.push_back(first_empty_log);


        for(block_id_t i = 0; i < total_block_num; i++){

            bm_->read_block(i + 1, log_buffer.data());
            for(int j = 0; j < entry_per_block; j++){

//                if(i * entry_per_block + j == 0){
//                    continue;
//                }
                if(i * entry_per_block + j >= log_size){
                    break;
                }
                auto *log_p = reinterpret_cast<Entry *>(log_buffer.data());
//                if(log_p[j].term == 0 && log_p[j].value == 0){
//                    break;
//                }
                Command cmd(log_p[j].value);
//                int index = log.size();
                log_entry<Command> new_log(log_p[j].index,log_p[j].term,cmd);
                log.push_back(new_log);
            }
        }

        if(meta_p[5] == 1){
            int snap_size = meta_p[6];
            int snap_block_num = snap_size % bm_->block_size() == 0 ?
                                 snap_size / bm_->block_size() : snap_size / bm_->block_size() + 1;
            for(int i = 0; i < snap_block_num; i++){
                bm_->read_block(i + snap_block_id, snap_buffer.data());
                for(int j = 0; j < bm_->block_size(); j++){
                    if(i * bm_->block_size() + j >= snap_size){
                        break;
                    }
                    snapshot.push_back(snap_buffer[j]);
                }
            }
        }
    }



}

template <typename Command>
void RaftLog<Command>::persist_meta(int currentTerm, int votedFor) {
    std::unique_lock<std::mutex> lock(mtx);

    std::vector<u8> meta_buffer(bm_->block_size());
    bm_->read_block(0, meta_buffer.data());
    int *meta_p = reinterpret_cast<int *>(meta_buffer.data());
    meta_p[0] = 1;
    meta_p[1] = currentTerm;
    meta_p[2] = votedFor;
}

template <typename Command>
void RaftLog<Command>::persist_log(std::vector<log_entry<Command>> log) {
    std::unique_lock<std::mutex> lock(mtx);

//    std::cerr<<"append "<<std::endl;
    int entry_per_block = bm_->block_size() / sizeof(Entry);
    int log_block_num = log.size() % entry_per_block == 0 ?
            log.size() / entry_per_block : log.size() / entry_per_block + 1;

    std::vector<u8> log_buffer(bm_->block_size());
    for(block_id_t i = 0; i < log_block_num; i++){

        bm_->read_block(i + 1, log_buffer.data());
        for(int j = 0; j < entry_per_block; j++){
            if( i * entry_per_block + j >= log.size()){
                break;
            }
//            if(i * entry_per_block + j == 0){
//
//                continue;
//            }
//            std::cerr<<"3"<<std::endl;
            auto *log_p = reinterpret_cast<Entry *>(log_buffer.data());
            log_p[j].index = log[i * entry_per_block + j].index;
            log_p[j].term = log[i * entry_per_block + j].term;
            if(i * entry_per_block + j == 0){
                continue;
            }
            log_p[j].value = log[i * entry_per_block + j].cmd.value;
//            std::cerr<<log_p[j].term<<" "<<log_p[j].value<<std::endl;

        }
        bm_->write_block(i+1, log_buffer.data());
        bm_->sync(i+1);
    }

    std::vector<u8> meta_buffer(bm_->block_size());
    bm_->read_block(0, meta_buffer.data());
    int *meta_p = reinterpret_cast<int *>(meta_buffer.data());
    meta_p[0] = 1;
    meta_p[3] = log_block_num;
    meta_p[4] = log.size();

    bm_->write_block(0, meta_buffer.data());
    bm_->sync(0);

}

template <typename Command>
void RaftLog<Command>::append_log(log_entry<Command> &entry) {
    std::unique_lock<std::mutex> lock(mtx);

//    std::cerr<<"append "<<entry.cmd.value;
    int entry_per_block = bm_->block_size() / sizeof(Entry);
    std::vector<u8> meta_buffer(bm_->block_size());
    std::vector<u8> log_buffer(bm_->block_size());
    bm_->read_block(0, meta_buffer.data());
    int *meta_p = reinterpret_cast<int *>(meta_buffer.data());

    int log_block_num = meta_p[3];
    int log_size = meta_p[4];
    block_id_t log_block_id;
    int block_entry_id;

    if(log_size % entry_per_block == 0){
        log_block_id = log_block_num + 1;
        block_entry_id = 0;
    }
    else{
        log_block_id = log_block_num;
        block_entry_id = log_size % entry_per_block;
    }
    bm_->read_block(log_block_id, log_buffer.data());
    auto *log_p = reinterpret_cast<Entry *>(log_buffer.data());
    log_p[block_entry_id].index = entry.index;
    log_p[block_entry_id].term = entry.term;
    log_p[block_entry_id].value = entry.cmd.value;

    meta_p[0] = 1;
    meta_p[3] = (int)log_block_id;
    meta_p[4] = log_size + 1;

    bm_->write_block(0, meta_buffer.data());
    bm_->write_block(log_block_id, log_buffer.data());
    bm_->sync(0);
    bm_->sync(log_block_id);

}
template <typename Command>
void RaftLog<Command>::append_logs(std::vector<log_entry<Command>>& log){

    for (auto &entry : log){
        append_log(entry);
    }
}

// 0 if persist log
// 1 currentTerm
// 2 votedFor
// 3 total_block_num
// 4 log size
// 5 if persist snapshot
// 6 snapshot size

template <typename Command>
void RaftLog<Command>::persist_snapshot(std::vector<u8> &snapshot){
    std::unique_lock<std::mutex> lock(mtx);

    const int snap_block_id = 17;
    std::vector<u8> meta_buffer(bm_->block_size());
    std::vector<u8> snap_buffer(bm_->block_size());
    bm_->read_block(0, meta_buffer.data());
    int *meta_p = reinterpret_cast<int *>(meta_buffer.data());

    int snap_block_num = snapshot.size() % bm_->block_size() == 0 ?
            snapshot.size() / bm_->block_size() : snapshot.size() / bm_->block_size() + 1;

    for(int i = 0; i < snap_block_num; i++){
        bm_->read_block(i + snap_block_id, snap_buffer.data());
        for(int j = 0; j < bm_->block_size(); j++){
            if(i * bm_->block_size() + j >= snapshot.size()){
                break;
            }
            snap_buffer[j] = snapshot[i * bm_->block_size() + j];
        }
        bm_->write_block(i + snap_block_id, snap_buffer.data());
        bm_->sync(i + snap_block_id);
    }

    meta_p[5] = 1;
    meta_p[6] = (int)snapshot.size();
    bm_->write_block(0, meta_buffer.data());
    bm_->sync(0);

}
} /* namespace chfs */
