#pragma once

#include "rsm/raft/log.h"
#include "rpc/msgpack.hpp"

namespace chfs {

const std::string RAFT_RPC_START_NODE = "start node";
const std::string RAFT_RPC_STOP_NODE = "stop node";
const std::string RAFT_RPC_NEW_COMMEND = "new commend";
const std::string RAFT_RPC_CHECK_LEADER = "check leader";
const std::string RAFT_RPC_IS_STOPPED = "check stopped";
const std::string RAFT_RPC_SAVE_SNAPSHOT = "save snapshot";
const std::string RAFT_RPC_GET_SNAPSHOT = "get snapshot";

const std::string RAFT_RPC_REQUEST_VOTE = "request vote";
const std::string RAFT_RPC_APPEND_ENTRY = "append entries";
const std::string RAFT_RPC_INSTALL_SNAPSHOT = "install snapshot";

struct RequestVoteArgs {
    /* Lab3: Your code here */
    //Part1
    int term;
    int candidateId;
    int lastLogIndex;
    int lastLogTerm;

    MSGPACK_DEFINE(
        term,
        candidateId,
        lastLogIndex,
        lastLogTerm
    )

    RequestVoteArgs(){}
    RequestVoteArgs(int term,int candidateId,int lastLogIndex,int lastLogTerm)
    :term(term),candidateId(candidateId),lastLogIndex(lastLogIndex),lastLogTerm(lastLogTerm){}
};

struct RequestVoteReply {
    /* Lab3: Your code here */
    //Part1
    int term;
    bool voteGranted;

    MSGPACK_DEFINE(
        term,
        voteGranted
    )

    RequestVoteReply(){}
    RequestVoteReply(int term,bool voteGranted)
    :term(term),voteGranted(voteGranted){}
};

template <typename Command>
struct AppendEntriesArgs {
    /* Lab3: Your code here */
    //Part1
    int term;
    int leaderId;
    int prevLogIndex;
    int prevLogTerm;
    std::vector<log_entry<Command>> entries;
    int leaderCommit;

    AppendEntriesArgs(){}
    AppendEntriesArgs(int term,int leaderId,int prevLogIndex,int prevLogTerm,std::vector<log_entry<Command>> entries,int leaderCommit)
    :term(term),leaderId(leaderId),prevLogIndex(prevLogIndex),prevLogTerm(prevLogTerm),entries(entries),leaderCommit(leaderCommit){}
};

struct RpcAppendEntriesArgs {
    /* Lab3: Your code here */

    int term;
    int leaderId;
    int prevLogIndex;
    int prevLogTerm;
    std::vector<int> entries_index;
    std::vector<int> entries_term;
    std::vector<int> entries_cmd;
    int leaderCommit;
    MSGPACK_DEFINE(
        term,
        leaderId,
        prevLogIndex,
        prevLogTerm,
        entries_index,
        entries_term,
        entries_cmd,
        leaderCommit
    )
    RpcAppendEntriesArgs(){}
};

template <typename Command>
RpcAppendEntriesArgs transform_append_entries_args(const AppendEntriesArgs<Command> &arg)
{
    /* Lab3: Your code here */
    RpcAppendEntriesArgs rpc_args;
    rpc_args.term = arg.term;
    rpc_args.leaderId = arg.leaderId;
    rpc_args.prevLogIndex = arg.prevLogIndex;
    rpc_args.prevLogTerm = arg.prevLogTerm;
    for(int i = 0; i < arg.entries.size(); i++){
        rpc_args.entries_index.push_back(arg.entries[i].index);
        rpc_args.entries_term.push_back(arg.entries[i].term);
        rpc_args.entries_cmd.push_back(arg.entries[i].cmd.value);
    }
    rpc_args.leaderCommit = arg.leaderCommit;

    return rpc_args;
//    return RpcAppendEntriesArgs();
}

template <typename Command>
AppendEntriesArgs<Command> transform_rpc_append_entries_args(const RpcAppendEntriesArgs &rpc_arg)
{
    /* Lab3: Your code here */
    AppendEntriesArgs<Command> args;
    args.term = rpc_arg.term;
    args.leaderId = rpc_arg.leaderId;
    args.prevLogIndex = rpc_arg.prevLogIndex;
    args.prevLogTerm = rpc_arg.prevLogTerm;
    for(int i = 0; i < rpc_arg.entries_index.size(); i++){
        log_entry<Command> tmp;
        args.entries.push_back(log_entry<Command>(rpc_arg.entries_index[i],rpc_arg.entries_term[i],rpc_arg.entries_cmd[i]));
    }
    args.leaderCommit = rpc_arg.leaderCommit;

    return args;
//    return AppendEntriesArgs<Command>();
}

struct AppendEntriesReply {
    /* Lab3: Your code here */
    //Part1
    int term;
    bool success;

    MSGPACK_DEFINE(
        term,
        success
    )

    AppendEntriesReply(){}
    AppendEntriesReply(int term,bool success)
    :term(term),success(success){}
};

struct InstallSnapshotArgs {
    /* Lab3: Your code here */

    int term;
    int leaderId;
    int lastIncludedIndex;
    int lastIncludedTerm;
    std::vector<u8> data;
    MSGPACK_DEFINE(
        term,
        leaderId,
        lastIncludedIndex,
        lastIncludedTerm,
        data
    )
    InstallSnapshotArgs(){}
};

struct InstallSnapshotReply {
    /* Lab3: Your code here */

    int term;
    MSGPACK_DEFINE(
        term
    )
    InstallSnapshotReply(){}
};

} /* namespace chfs */