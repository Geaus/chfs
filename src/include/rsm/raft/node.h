#pragma once

#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include <algorithm>
#include <thread>
#include <memory>
#include <stdarg.h>
#include <unistd.h>
#include <filesystem>

#include "rsm/state_machine.h"
#include "rsm/raft/log.h"
#include "rsm/raft/protocol.h"
#include "utils/thread_pool.h"
#include "librpc/server.h"
#include "librpc/client.h"
#include "block/manager.h"

namespace chfs {

enum class RaftRole {
    Follower,
    Candidate,
    Leader
};

struct RaftNodeConfig {
    int node_id;
    uint16_t port;
    std::string ip_address;
};

template <typename StateMachine, typename Command>
class RaftNode {

#define RAFT_LOG(fmt, args...)                                                                                   \
    do {                                                                                                         \
        auto now =                                                                                               \
            std::chrono::duration_cast<std::chrono::milliseconds>(                                               \
                std::chrono::system_clock::now().time_since_epoch())                                             \
                .count();                                                                                        \
        char buf[512];                                                                                      \
        sprintf(buf,"[%ld][%s:%d][node %d term %d role %d] " fmt "\n", now, __FILE__, __LINE__, my_id, current_term, role, ##args); \
        thread_pool->enqueue([=]() { std::cerr << buf;} );                                         \
    } while (0);

public:
    RaftNode (int node_id, std::vector<RaftNodeConfig> node_configs);
    ~RaftNode();

    /* interfaces for test */
    void set_network(std::map<int, bool> &network_availablility);
    void set_reliable(bool flag);
    int get_list_state_log_num();
    int rpc_count();
    std::vector<u8> get_snapshot_direct();

private:
    /* 
     * Start the raft node.
     * Please make sure all of the rpc request handlers have been registered before this method.
     */
    auto start() -> int;

    /*
     * Stop the raft node.
     */
    auto stop() -> int;
    
    /* Returns whether this node is the leader, you should also return the current term. */
    auto is_leader() -> std::tuple<bool, int>;

    /* Checks whether the node is stopped */
    auto is_stopped() -> bool;

    /* 
     * Send a new command to the raft nodes.
     * The returned tuple of the method contains three values:
     * 1. bool:  True if this raft node is the leader that successfully appends the log,
     *      false If this node is not the leader.
     * 2. int: Current term.
     * 3. int: Log index.
     */
    auto new_command(std::vector<u8> cmd_data, int cmd_size) -> std::tuple<bool, int, int>;

    /* Save a snapshot of the state machine and compact the log. */
    auto save_snapshot() -> bool;

    /* Get a snapshot of the state machine */
    auto get_snapshot() -> std::vector<u8>;


    /* Internal RPC handlers */
    auto request_vote(RequestVoteArgs arg) -> RequestVoteReply;
    auto append_entries(RpcAppendEntriesArgs arg) -> AppendEntriesReply;
    auto install_snapshot(InstallSnapshotArgs arg) -> InstallSnapshotReply;

    /* RPC helpers */
    void send_request_vote(int target, RequestVoteArgs arg);
    void handle_request_vote_reply(int target, const RequestVoteArgs arg, const RequestVoteReply reply);

    void send_append_entries(int target, AppendEntriesArgs<Command> arg);
    void handle_append_entries_reply(int target, const AppendEntriesArgs<Command> arg, const AppendEntriesReply reply);

    void send_install_snapshot(int target, InstallSnapshotArgs arg);
    void handle_install_snapshot_reply(int target, const InstallSnapshotArgs arg, const InstallSnapshotReply reply);

    /* background workers */
    void run_background_ping();
    void run_background_election();
    void run_background_commit();
    void run_background_apply();

    /* help function */
    long long get_timestamp();
    void get_random_timeout();
    void convert_to_follower(int term);
    void start_new_election();
    void send_heartbeat();
    int snapshot_index();
    int snapshot_term();

    /* Data structures */
    bool network_stat;          /* for test */

    std::mutex mtx;                             /* A big lock to protect the whole data structure. */
    std::mutex clients_mtx;                     /* A lock to protect RpcClient pointers */
    std::unique_ptr<ThreadPool> thread_pool;
    std::unique_ptr<RaftLog<Command>> log_storage;     /* To persist the raft log. */
    std::unique_ptr<StateMachine> state;  /*  The state machine that applies the raft log, e.g. a kv store. */

    std::unique_ptr<RpcServer> rpc_server;      /* RPC server to recieve and handle the RPC requests. */
    std::map<int, std::unique_ptr<RpcClient>> rpc_clients_map;  /* RPC clients of all raft nodes including this node. */
    std::vector<RaftNodeConfig> node_configs;   /* Configuration for all nodes */ 
    int my_id;                                  /* The index of this node in rpc_clients, start from 0. */

    std::atomic_bool stopped;

    RaftRole role;
    int current_term;
    int leader_id;

    std::unique_ptr<std::thread> background_election;
    std::unique_ptr<std::thread> background_ping;
    std::unique_ptr<std::thread> background_commit;
    std::unique_ptr<std::thread> background_apply;

    bool init = true;
    int votedFor;
    std::vector<log_entry<Command>> log;

    int commitIndex;
    int lastApplied;

    std::vector<int> nextIndex;
    std::vector<int> matchIndex;
    std::vector<int> matchCount;

    std::vector< bool > vote_result;

    long long election_timer;

    int follower_election_timeout;
    int candidate_election_timeout;

    std::vector<u8> snapshot;

    /* Lab3: Your code here */
};

template <typename StateMachine, typename Command>
RaftNode<StateMachine, Command>::RaftNode(int node_id, std::vector<RaftNodeConfig> configs):
    network_stat(true),
    node_configs(configs),
    my_id(node_id),
    stopped(true),
    role(RaftRole::Follower),
    current_term(0),
    leader_id(-1)
{
    auto my_config = node_configs[my_id];

    /* launch RPC server */
    rpc_server = std::make_unique<RpcServer>(my_config.ip_address, my_config.port);

    /* Register the RPCs. */
    rpc_server->bind(RAFT_RPC_START_NODE, [this]() { return this->start(); });
    rpc_server->bind(RAFT_RPC_STOP_NODE, [this]() { return this->stop(); });
    rpc_server->bind(RAFT_RPC_CHECK_LEADER, [this]() { return this->is_leader(); });
    rpc_server->bind(RAFT_RPC_IS_STOPPED, [this]() { return this->is_stopped(); });
    rpc_server->bind(RAFT_RPC_NEW_COMMEND, [this](std::vector<u8> data, int cmd_size) { return this->new_command(data, cmd_size); });
    rpc_server->bind(RAFT_RPC_SAVE_SNAPSHOT, [this]() { return this->save_snapshot(); });
    rpc_server->bind(RAFT_RPC_GET_SNAPSHOT, [this]() { return this->get_snapshot(); });

    rpc_server->bind(RAFT_RPC_REQUEST_VOTE, [this](RequestVoteArgs arg) { return this->request_vote(arg); });
    rpc_server->bind(RAFT_RPC_APPEND_ENTRY, [this](RpcAppendEntriesArgs arg) { return this->append_entries(arg); });
    rpc_server->bind(RAFT_RPC_INSTALL_SNAPSHOT, [this](InstallSnapshotArgs arg) { return this->install_snapshot(arg); });

    /* Lab3: Your code here */


    thread_pool = std::make_unique<ThreadPool>(32);
    state = std::make_unique<StateMachine>();

    std::shared_ptr<BlockManager> bm = std::make_shared<BlockManager>("/tmp/raft_log/node_"+std::to_string(my_id));
    log_storage = std::make_unique<RaftLog<Command>>(bm);
    log_storage->restore(current_term,votedFor,log,snapshot);


//    votedFor = -1;
//    log_entry<Command> first_empty_log = log_entry<Command>();
//    log.push_back(first_empty_log);

    commitIndex = snapshot_index();
    lastApplied = snapshot_index();

    if(!snapshot.empty()) {
        state->apply_snapshot(snapshot);
    }
    vote_result.assign(node_configs.size(), false);

    election_timer = get_timestamp();
    get_random_timeout();

    rpc_server->run(true, configs.size());
}

template <typename StateMachine, typename Command>
RaftNode<StateMachine, Command>::~RaftNode()
{
    stop();

    thread_pool.reset();
    rpc_server.reset();
    state.reset();
    log_storage.reset();
}

/******************************************************************

                        RPC Interfaces

*******************************************************************/


template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::start() -> int
{
    /* Lab3: Your code here */
    //Part1
    RAFT_LOG("start");
    std::map<int, bool> node_network_available;
    for (auto config: node_configs) {
        node_network_available.insert(std::make_pair(config.node_id, true));
    }
    set_network(node_network_available);

    if(init){
        while(true) {
            bool flag = true;
            for (const auto &client: rpc_clients_map) {
                if(client.second != nullptr && client.second->get_connection_state() != rpc::client::connection_state::connected){
                    flag = false;
                }
            }
            if(flag){
                break;
            }
        }
        init = false;
    }


    stopped.store(false);
    background_election = std::make_unique<std::thread>(&RaftNode::run_background_election, this);
    background_ping = std::make_unique<std::thread>(&RaftNode::run_background_ping, this);
    background_commit = std::make_unique<std::thread>(&RaftNode::run_background_commit, this);
    background_apply = std::make_unique<std::thread>(&RaftNode::run_background_apply, this);

    return 0;
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::stop() -> int
{
    /* Lab3: Your code here */
    //Part1
    RAFT_LOG("stop");
//    for(const auto &entry : log){
//       std::cerr<<entry.index<<std::endl;
//    }

    stopped.store(true);
    background_election->join();
    background_ping->join();
    background_commit->join();
    background_apply->join();

    return 0;
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::is_leader() -> std::tuple<bool, int>
{
    /* Lab3: Your code here */
    //Part1
    bool is_leader = role == RaftRole::Leader;
    return std::make_tuple(is_leader, current_term);

//    return std::make_tuple(false, -1);
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::is_stopped() -> bool
{
    return stopped.load();
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::new_command(std::vector<u8> cmd_data, int cmd_size) -> std::tuple<bool, int, int>
{
    /* Lab3: Your code here */
    std::unique_lock<std::mutex> lock(mtx);

    bool flag = false;
    int term = -1;
    int index = -1;

    if(std::get<0>(is_leader())) {
        Command cmd;
        cmd.deserialize(cmd_data,cmd_size);

        index = log.back().index + 1;
        log_entry<Command> new_log(index,current_term,cmd);

        log.push_back(new_log);
//        RAFT_LOG("leader %d add log index %d value %d",my_id,index,cmd.value);

        nextIndex[my_id] = index + 1;
        matchIndex[my_id] = index;
        matchCount.push_back(1);

        flag = true;
        term = current_term;

//        log_storage->persist_log(log);
         log_storage->append_log(new_log);
//        RAFT_LOG("add finish")
    }


    return std::make_tuple(flag, term, index);
//    return std::make_tuple(false, -1, -1);
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::save_snapshot() -> bool
{
    /* Lab3: Your code here */
    std::unique_lock<std::mutex> lock(mtx);

    snapshot = state->snapshot();
    if(lastApplied <= log.back().index) {

        int index = log[lastApplied - snapshot_index()].index;
        int term = log[lastApplied - snapshot_index()].term;

        log.erase(log.begin()+1, log.begin() + lastApplied + 1 - snapshot_index());

        log.front().index = index;
        log.front().term = term;

    }
    else {
        int index = log[lastApplied - snapshot_index()].index;
        int term = log[lastApplied - snapshot_index()].term;

        log.erase(log.begin()+1, log.end() + 1);
        log.front().index = index;
        log.front().term = term;
    }

    RAFT_LOG("node %d save snapshot %d",my_id,(int)snapshot.size())

    log_storage->persist_snapshot(snapshot);
    log_storage->persist_log(log);
    return true;
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::get_snapshot() -> std::vector<u8>
{
    /* Lab3: Your code here */
    std::unique_lock<std::mutex> lock(mtx);

    return state->snapshot();
    return std::vector<u8>();
}

/******************************************************************

                         Internal RPC Related

*******************************************************************/


template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::request_vote(RequestVoteArgs args) -> RequestVoteReply
{
    /* Lab3: Your code here */
    //Part1
    std::unique_lock<std::mutex> lock(mtx);
    election_timer = get_timestamp();
    std::cerr<<"1"<<std::endl;

    RequestVoteReply reply;
    reply.term = current_term;
    reply.voteGranted = false;

    if (args.term < current_term) {
        return reply;
    }
    if(args.term > current_term) {
        convert_to_follower(args.term);
    }
    if(votedFor == -1 || votedFor == args.candidateId) {
        if(args.lastLogTerm > log.back().term ||
        (args.lastLogTerm == log.back().term && args.lastLogIndex >= log.back().index)) {

            votedFor = args.candidateId;
            reply.voteGranted = true;

            log_storage->persist_meta(current_term,votedFor);
        }
    }
    return reply;


//    return RequestVoteReply();
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::handle_request_vote_reply(int target, const RequestVoteArgs arg, const RequestVoteReply reply)
{
    /* Lab3: Your code here */
    //Part1
    std::unique_lock<std::mutex> lock(mtx);
    election_timer = get_timestamp();
    std::cerr<<"2"<<std::endl;

    if(reply.term > current_term) {
        convert_to_follower(reply.term);
    }
    if(role == RaftRole::Candidate) {

        vote_result[target] = reply.voteGranted;
//        RAFT_LOG("node %d get vote from node %d, result : %d", arg.candidateId, target, reply.voteGranted);
        int node_num = rpc_clients_map.size();
        int votes = std::accumulate(vote_result.begin(),vote_result.end(),0);

        if(votes > node_num / 2){
            role = RaftRole::Leader;
            RAFT_LOG("node %d become leader",my_id)
            nextIndex.assign(node_num,log.back().index + 1);
            matchIndex.assign(node_num,0);
            matchIndex[my_id] = log.back().index;
            matchCount.assign(log.back().index - commitIndex, 0);

            send_heartbeat();
        }
    }
    return;
}

template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::append_entries(RpcAppendEntriesArgs rpc_arg) -> AppendEntriesReply
{
    /* Lab3: Your code here */
    //Part1
    std::unique_lock<std::mutex> lock(mtx);
    election_timer = get_timestamp();
    std::cerr<<"3"<<std::endl;

    AppendEntriesReply reply;
    reply.term = current_term;
    reply.success = false;

    AppendEntriesArgs<Command> args = transform_rpc_append_entries_args<Command>(rpc_arg);


    if(args.term < current_term) {
        return reply;
    }
    if(args.term > current_term || role == RaftRole::Candidate) {
        convert_to_follower(args.term);
    }

    if(!(args.prevLogIndex <= log.back().index && args.prevLogTerm == log[args.prevLogIndex-snapshot_index()].term)) {
        return reply;
    }
    else {
        if (args.entries.empty()) {
            reply.success = true;
        }
        else{
            if (args.prevLogIndex < log.back().index) {

                log.erase(log.begin() + args.prevLogIndex + 1 - snapshot_index(), log.end());
                log.insert(log.end(), args.entries.begin(), args.entries.end());
                log_storage->persist_log(log);
//                RAFT_LOG("node %d receive log %d",my_id,args.entries[0].cmd.value)
            }
            else {

                log.insert(log.end(), args.entries.begin(), args.entries.end());
                log_storage->append_logs(args.entries);
//                log_storage->persist_log(log);
//                RAFT_LOG("node %d receive log %d",my_id,args.entries[0].cmd.value)
            }
            reply.success = true;
        }
        if (args.leaderCommit > commitIndex) {
            commitIndex = std::min(args.leaderCommit, log.back().index);
        }
    }

    return reply;

    return AppendEntriesReply();
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::handle_append_entries_reply(int node_id, const AppendEntriesArgs<Command> arg, const AppendEntriesReply reply)
{
    /* Lab3: Your code here */
    //Part1
    std::unique_lock<std::mutex> lock(mtx);
    election_timer = get_timestamp();
    std::cerr<<"4"<<std::endl;

    if(reply.term > current_term) {
        convert_to_follower(reply.term);
        return;
    }
    if(!std::get<0>(is_leader())) {
        return;
    }
    if(reply.success) {
        int prev_match = matchIndex[node_id];
        matchIndex[node_id] = std::max(matchIndex[node_id], (int)(arg.prevLogIndex + arg.entries.size()));
        nextIndex[node_id] = matchIndex[node_id] + 1;


        int end_index = std::max(prev_match - commitIndex, 0) - 1;
        int node_num = rpc_clients_map.size();
        for (int i = matchIndex[node_id] - commitIndex - 1; i > end_index; i--) {
            matchCount[i] += 1;
            if (log[commitIndex + i + 1 - snapshot_index()].term == current_term && matchCount[i] > node_num / 2) {
                commitIndex += i + 1;
                matchCount.erase(matchCount.begin(), matchCount.begin() + i + 1);
                break;
            }
        }
    }
    else {
        nextIndex[node_id] = std::min(nextIndex[node_id], arg.prevLogIndex);
    }
    return;
}


template <typename StateMachine, typename Command>
auto RaftNode<StateMachine, Command>::install_snapshot(InstallSnapshotArgs args) -> InstallSnapshotReply
{
    /* Lab3: Your code here */
    std::unique_lock<std::mutex> lock(mtx);
    election_timer = get_timestamp();
    std::cerr<<"5"<<std::endl;

    InstallSnapshotReply reply;
    reply.term = current_term;

    if (args.term < current_term) {
        return reply;
    }
    if (args.term > current_term || role == RaftRole::Candidate) {
        convert_to_follower(args.term);
    }
    if (!(args.lastIncludedIndex <= log.back().index && args.lastIncludedTerm == log[args.lastIncludedIndex - snapshot_index()].term)) {

        log.assign(1, log_entry<Command>(args.lastIncludedIndex, args.lastIncludedTerm));
    }
    else {
        int index = log[args.lastIncludedIndex - snapshot_index()].index;
        int term = log[args.lastIncludedIndex - snapshot_index()].term;

        log.erase(log.begin()+1, log.begin() + args.lastIncludedIndex + 1 - snapshot_index());

        log.front().index = index;
        log.front().term = term;
    }
    RAFT_LOG("node %d receive snapshot %d",my_id,(int)args.data.size())


    lastApplied = args.lastIncludedIndex;
    commitIndex = std::max(commitIndex,args.lastIncludedIndex);

    snapshot = args.data;
    state->apply_snapshot(snapshot);

    log_storage->persist_snapshot(args.data);
    log_storage->persist_log(log);


    return InstallSnapshotReply();
}


template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::handle_install_snapshot_reply(int node_id, const InstallSnapshotArgs arg, const InstallSnapshotReply reply)
{
    /* Lab3: Your code here */
    std::unique_lock<std::mutex> lock(mtx);
    std::cerr<<"6"<<std::endl;

    if (reply.term > current_term) {
        convert_to_follower(reply.term);
        return;
    }
    if (!std::get<0>(is_leader())) {
        return;
    }

    matchIndex[node_id] = std::max(matchIndex[node_id], arg.lastIncludedIndex);
    nextIndex[node_id] = matchIndex[node_id] + 1;
    return;
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::send_request_vote(int target_id, RequestVoteArgs arg)
{
    std::unique_lock<std::mutex> clients_lock(clients_mtx);
    if (rpc_clients_map[target_id] == nullptr
        || rpc_clients_map[target_id]->get_connection_state() != rpc::client::connection_state::connected) {
        return;
    }

    auto res = rpc_clients_map[target_id]->call(RAFT_RPC_REQUEST_VOTE, arg);
    clients_lock.unlock();
    if (res.is_ok()) {
        handle_request_vote_reply(target_id, arg, res.unwrap()->as<RequestVoteReply>());
    } else {
        // RPC fails
    }
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::send_append_entries(int target_id, AppendEntriesArgs<Command> arg)
{
    std::unique_lock<std::mutex> clients_lock(clients_mtx);
    if (rpc_clients_map[target_id] == nullptr 
        || rpc_clients_map[target_id]->get_connection_state() != rpc::client::connection_state::connected) {
        return;
    }

    RpcAppendEntriesArgs rpc_arg = transform_append_entries_args(arg);
    auto res = rpc_clients_map[target_id]->call(RAFT_RPC_APPEND_ENTRY, rpc_arg);
    clients_lock.unlock();
    if (res.is_ok()) {
        handle_append_entries_reply(target_id, arg, res.unwrap()->as<AppendEntriesReply>());
    } else {
        // RPC fails
    }
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::send_install_snapshot(int target_id, InstallSnapshotArgs arg)
{
    std::unique_lock<std::mutex> clients_lock(clients_mtx);
    if (rpc_clients_map[target_id] == nullptr
        || rpc_clients_map[target_id]->get_connection_state() != rpc::client::connection_state::connected) {
        return;
    }

    auto res = rpc_clients_map[target_id]->call(RAFT_RPC_INSTALL_SNAPSHOT, arg);
    clients_lock.unlock();
    if (res.is_ok()) { 
        handle_install_snapshot_reply(target_id, arg, res.unwrap()->as<InstallSnapshotReply>());
    } else {
        // RPC fails
    }
}


/******************************************************************

                        Background Workers

*******************************************************************/

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::run_background_election() {
    // Periodly check the liveness of the leader.
    //Part1
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    // Work for followers and candidates.

    /* Uncomment following code when you finish */

     while (true){
         if (is_stopped()) {
             return;
         }
         /* Lab3: Your code here */

         lock.lock();
         long long current_time = get_timestamp();
         switch (role) {
             case RaftRole::Follower:{
                 if(current_time - election_timer > follower_election_timeout) {
                       start_new_election();
//                       RAFT_LOG("node %d become candidate",my_id)
                 }
                 break;
             }
             case RaftRole::Candidate:{
                 if(current_time - election_timer > candidate_election_timeout){
                      start_new_election();
                 }
                 break;
              }
             default:
                 break;

         }
         lock.unlock();

         std::this_thread::sleep_for(std::chrono::milliseconds(10));
     }
return;
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::run_background_commit() {
    // Periodly send logs to the follower.
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    // Only work for the leader.

    /* Uncomment following code when you finish */
     while (true) {

         if (is_stopped()) {
             return;
         }
         /* Lab3: Your code here */

         lock.lock();
         if (std::get<0>(is_leader())) {

             for(const auto &client : rpc_clients_map) {
                 if(client.first == my_id || client.second == nullptr) {
                     continue;
                 }
                 if(log.back().index >= nextIndex[client.first]) {
                     if(nextIndex[client.first] <= snapshot_index()) {
                         InstallSnapshotArgs args(current_term,my_id,snapshot_index(),snapshot_term(),snapshot);
                         RAFT_LOG("node %d send snapshot %d to node %d",my_id,(int)snapshot.size(),client.first)
                         thread_pool->enqueue(&RaftNode::send_install_snapshot,this,client.first,args);

                     }
                     else {
                         std::vector<log_entry<Command>> ret;
                         ret.assign(log.begin()+nextIndex[client.first]-snapshot_index(),
                                    log.begin()+log.back().index + 1 - snapshot_index());

                         if(nextIndex[client.first] - 1 - snapshot_index() < log.size()) {
                             AppendEntriesArgs<Command> args(current_term,my_id,nextIndex[client.first] - 1,
                                                             log[nextIndex[client.first] - 1 - snapshot_index()].term,ret,commitIndex);
                             thread_pool->enqueue(&RaftNode::send_append_entries,this,client.first,args);
                         }
                     }
                 }
             }
         }
         lock.unlock();

         std::this_thread::sleep_for(std::chrono::milliseconds(50));
     }

    return;
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::run_background_apply() {
    // Periodly apply committed logs the state machine
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    // Work for all the nodes.

    /* Uncomment following code when you finish */
     while (true) {

         if (is_stopped()) {
             return;
         }
         /* Lab3: Your code here */
         lock.lock();
         if (commitIndex > lastApplied) {

             for (int i = lastApplied + 1; i <= commitIndex; i++) {
//                 RAFT_LOG("node %d apply index %d value %d",my_id,i,log[i - snapshot_index()].cmd.value)
                 state->apply_log(log[i - snapshot_index()].cmd);
             }
             lastApplied = commitIndex;
         }
         lock.unlock();

         std::this_thread::sleep_for(std::chrono::milliseconds(10));
     }

    return;
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::run_background_ping() {
    // Periodly send empty append_entries RPC to the followers.
    //Part1
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    // Only work for the leader.

    /* Uncomment following code when you finish */
     while (true) {

         if (is_stopped()) {
             return;
         }
         /* Lab3: Your code here */
         lock.lock();
         if(std::get<0>(is_leader())) {
             send_heartbeat();
         }
         lock.unlock();

         std::this_thread::sleep_for(std::chrono::milliseconds(150));

     }

    return;
}

/******************************************************************

                          Test Functions (must not edit)

*******************************************************************/

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::set_network(std::map<int, bool> &network_availability)
{
    std::unique_lock<std::mutex> clients_lock(clients_mtx);

    /* turn off network */
    if (!network_availability[my_id]) {
        for (auto &&client: rpc_clients_map) {
            if (client.second != nullptr)
                client.second.reset();
        }

        return;
    }

    for (auto node_network: network_availability) {
        int node_id = node_network.first;
        bool node_status = node_network.second;

        if (node_status && rpc_clients_map[node_id] == nullptr) {
            RaftNodeConfig target_config;
            for (auto config: node_configs) {
                if (config.node_id == node_id) 
                    target_config = config;
            }

            rpc_clients_map[node_id] = std::make_unique<RpcClient>(target_config.ip_address, target_config.port, true);
        }

        if (!node_status && rpc_clients_map[node_id] != nullptr) {
            rpc_clients_map[node_id].reset();
        }
    }
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::set_reliable(bool flag)
{
    std::unique_lock<std::mutex> clients_lock(clients_mtx);
    for (auto &&client: rpc_clients_map) {
        if (client.second) {
            client.second->set_reliable(flag);
        }
    }
}

template <typename StateMachine, typename Command>
int RaftNode<StateMachine, Command>::get_list_state_log_num()
{
    /* only applied to ListStateMachine*/
    std::unique_lock<std::mutex> lock(mtx);

    return state->num_append_logs;
}

template <typename StateMachine, typename Command>
int RaftNode<StateMachine, Command>::rpc_count()
{
    int sum = 0;
    std::unique_lock<std::mutex> clients_lock(clients_mtx);

    for (auto &&client: rpc_clients_map) {
        if (client.second) {
            sum += client.second->count();
        }
    }
    
    return sum;
}

template <typename StateMachine, typename Command>
std::vector<u8> RaftNode<StateMachine, Command>::get_snapshot_direct()
{
    if (is_stopped()) {
        return std::vector<u8>();
    }

    std::unique_lock<std::mutex> lock(mtx);

    return state->snapshot(); 
}

/*********************************************************************
 *
 * @tparam StateMachine
 * @tparam Command
 * help function
***********************************************************************/
template <typename StateMachine, typename Command>
long long RaftNode<StateMachine, Command>::get_timestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::nanoseconds d = now.time_since_epoch();
    std::chrono::milliseconds mill = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    return mill.count();
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::get_random_timeout() {
    static std::random_device rd;
    static std::minstd_rand gen(rd());

    static std::uniform_int_distribution<int> follower(300, 500); // Adjust param
    static std::uniform_int_distribution<int> candidate(800, 1000); // Adjust param

    follower_election_timeout = follower(gen);
    candidate_election_timeout = candidate(gen);
}

template <typename StateMachine, typename Command>
void  RaftNode<StateMachine, Command>::convert_to_follower(int term) {

    role = RaftRole::Follower;
    current_term = term;
    votedFor = -1;
    log_storage->persist_meta(current_term,votedFor);

    get_random_timeout();
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::start_new_election() {

    role = RaftRole::Candidate;
    current_term++;
    votedFor = my_id;
    vote_result.assign(rpc_clients_map.size(), false);
    vote_result[my_id] = true;

    log_storage->persist_meta(current_term,votedFor);
    get_random_timeout();

    RequestVoteArgs args(current_term, my_id, log.back().index, log.back().term);

    for(const auto &client : rpc_clients_map) {
        if(client.first == my_id || client.second == nullptr) {
            continue;
        }
        thread_pool->enqueue(&RaftNode::send_request_vote,this,client.first,args);
    }
    election_timer = get_timestamp();
}

template <typename StateMachine, typename Command>
void RaftNode<StateMachine, Command>::send_heartbeat() {

    for(const auto &client : rpc_clients_map) {
        if(client.first == my_id || client.second == nullptr) {
            continue;
        }

        if(nextIndex[client.first] - 1 - snapshot_index() < log.size()) {
            AppendEntriesArgs<Command> args(current_term,my_id,nextIndex[client.first] - 1,
                                            log[nextIndex[client.first] - 1 - snapshot_index()].term,std::vector<log_entry<Command>>(),commitIndex);

//        RAFT_LOG("ping")
            thread_pool->enqueue(&RaftNode::send_append_entries,this,client.first,args);
        }

    }

}
template <typename StateMachine, typename Command>
int RaftNode<StateMachine, Command>::snapshot_index(){
    return log.front().index;
}

template <typename StateMachine, typename Command>
int RaftNode<StateMachine, Command>::snapshot_term(){
    return log.front().term;
}

}