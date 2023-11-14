#include "gtest/gtest.h"
#include "rsm/raft/node.h"
#include "librpc/client.h"
#include "rsm/list_state_machine.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace chfs {

/* !! elements' vector index equal to id for convenience */

const uint16_t PORT_BASE = 8080;

void mssleep(int ms)
{
  usleep(ms * 1000);
}

void delete_directory_contents(const fs::path& dir)
{
  for (const auto& entry : fs::directory_iterator(dir)) 
    fs::remove_all(entry.path());
}

void prepare_environment(const fs::path& dir)
{
  fs::create_directories(dir);
  delete_directory_contents(dir);
}

std::vector<RaftNodeConfig> generate_configs(int num)
{
  std::vector<RaftNodeConfig> configs;

  for (int i = 0; i < num; i++) {
    RaftNodeConfig config = {
      .node_id = i,
      .port = static_cast<uint16_t>(PORT_BASE + i),
      .ip_address = "127.0.0.1"
    };

    configs.push_back(config);
  }

  return configs;
}

class RaftTest: public::testing::Test {
protected:
  // This function is called before every test.
  void SetUp() override {
    retry_time = 10;
  }

  // This function is called after every test.
  void TearDown() override {
    for (auto config: configs) {
      node_network_available[config.node_id] = false;
    }
    DisableNode(0);
  }

public:
  std::vector<RaftNodeConfig> configs;
  std::vector<std::unique_ptr<RaftNode<ListStateMachine, ListCommand>>> nodes;
  std::vector<std::unique_ptr<RpcClient>> clients;
  std::vector<std::unique_ptr<ListStateMachine>> states;
  std::mutex mtx;  /* protect states */

  int retry_time;

  /* true for good network */
  std::map<int, bool> node_network_available;

  void InitNodes(int node_num)
  {
    configs = generate_configs(node_num);
    prepare_environment("/tmp/raft_log");
    for (auto config: configs) {
      nodes.push_back(std::make_unique<RaftNode<ListStateMachine, ListCommand>>(config.node_id, configs));
      states.push_back(std::make_unique<ListStateMachine>());
      node_network_available.insert(std::make_pair(config.node_id, true));
    }

    for (auto config: configs) {
      clients.push_back(std::make_unique<RpcClient>(config.ip_address, config.port, true));
    }

    for (auto &&client: clients) {
      while (client->get_connection_state() != rpc::client::connection_state::connected) {
        sleep(1);
      }
      
      /* Start Node */
      auto res = client->call(RAFT_RPC_START_NODE);
      EXPECT_EQ(res.is_ok(), true);
      EXPECT_EQ(res.unwrap()->as<int>(), 0);
    }
  }

  int CheckOneLeader()
  {
    for (int i = 0 ; i < retry_time; i++) {
      std::map<int, int> term_leaders;
      for (int j = 0; j < configs.size(); j++) {
        if (!node_network_available[j]) {
          continue;
        }

        auto res2 = clients[j]->call(RAFT_RPC_CHECK_LEADER);
        std::tuple<bool, int> flag_term = res2.unwrap()->as<std::tuple<bool, int>>();
        if (std::get<0>(flag_term)) {
          int term = std::get<1>(flag_term);
          EXPECT_GT(term, 0) << "leader term number invalid";
          EXPECT_EQ(term_leaders.find(term), term_leaders.end()) << "term " << term << " has more than one leader";
          term_leaders[term] = configs[j].node_id;
        }
      }

      if (term_leaders.size() > 0) {
        auto last_term = term_leaders.rbegin();
        return last_term->second;
      }

      mssleep(rand() % 300 + 900);
    }

    ADD_FAILURE() << "There is no leader";
    return -1;
  }

  void CheckNoLeader()
  {
    int num_nodes = configs.size();
    for (int i = 0; i < num_nodes; i++) {
      if (!node_network_available[i]) {
        continue;
      }

      auto res2 = clients[i]->call(RAFT_RPC_CHECK_LEADER);
      auto flag_term = res2. unwrap()->as<std::tuple<bool, int>>();
      EXPECT_EQ(std::get<0>(flag_term), false) << "Node " << i << " is leader, which is unexpected";
    }
  }

  int CheckSameTerm()
  {
    int current_term = -1;
    int num_nodes = configs.size();
    for (int i = 0; i < num_nodes; i++) {
      auto res1 = clients[i]->call(RAFT_RPC_CHECK_LEADER);
      auto flag_term = res1.unwrap()->as<std::tuple<bool, int>>();
      int term = std::get<1>(flag_term);
      EXPECT_GE(term, 0) << "term number invalid";

      if (current_term == -1) {
        current_term = term;
      }

      EXPECT_EQ(current_term, term) << "inconsistent term";
    }

    return current_term;
  }

  int GetCommittedValue(int log_idx)
  {
    for (size_t i = 0; i < configs.size(); i++) {
      auto snapshot = nodes[i]->get_snapshot_direct();
      std::unique_lock<std::mutex> lock(mtx);
      states[i]->apply_snapshot(snapshot); 

      int log_value;
      if (static_cast<int>(states[i]->store.size() > log_idx)) {
        log_value = states[i]->store[log_idx];
        return log_value;
      }
    }

    ADD_FAILURE() << "log " << log_idx << " is not committed";
    return -1;
  }

  int NumCommitted(int log_idx)
  {
    int cnt = 0;
    int old_value = 0;
    for (size_t i = 0; i < configs.size(); i++) {
      bool has_log;
      int log_value;

      auto snapshot = nodes[i]->get_snapshot_direct();

      std::unique_lock<std::mutex> lock(mtx);
      states[i]->apply_snapshot(snapshot);

      // std::cerr << "node " << i << " snapshot: ";
      // for (auto c: snapshot) {
      //   std::cerr << c;
      // }
      // std::cerr << std::endl;

      if (static_cast<int>(states[i]->store.size()) > log_idx) {
        log_value = states[i]->store[log_idx];
        has_log = true;
      } else {
        has_log = false;
      }

      lock.unlock();

      if (has_log) {
        cnt++;
        if (cnt == 1) {
          old_value = log_value;
        } else {
          EXPECT_EQ(old_value, log_value) << "node " << i << " has inconsistent log value: (" << log_value << ", " << old_value << ") at idx " << log_idx;
        }
      }
    }

    return cnt;
  }
  
  int AppendNewCommand(int value, int expected_nodes)
  {
    ListCommand cmd(value);
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::seconds(3 * retry_time + rand() % 15);
    int leader_idx = 0;
    while (std::chrono::system_clock::now() < end) {
      int log_idx = -1;
      for (size_t i = 0; i < configs.size(); i++) {
        leader_idx = (leader_idx + 1) % configs.size();

        if (!node_network_available[leader_idx]) 
          continue;
        
        auto res2 = clients[leader_idx]->call(RAFT_RPC_NEW_COMMEND, cmd.serialize(cmd.size()), cmd.size());
        auto ret2 = res2.unwrap()->as<std::tuple<bool, int, int>>();

        bool is_leader;
        int temp_term;
        int temp_idx;
        std::tie(is_leader, temp_term, temp_idx) = ret2;

        if (is_leader) {
          log_idx = temp_idx;
          break;
        }
      }

      if (log_idx != -1) {
        auto check_start = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() < check_start + std::chrono::seconds(2)) {
          int committed_nodes = NumCommitted(log_idx);
          if (committed_nodes >= expected_nodes) {
            /* The log is committed */
            int committed_value = GetCommittedValue(log_idx);
            if (committed_value == value) {
              return log_idx;
            }
          }

          mssleep(20);
        }
      } else {
        /* no leader */
        mssleep(50);
      }

    }

    ADD_FAILURE() << "Cannot make agreement";
    return -1;
  }

  int WaitCommit(int index, int num_committed_server, int start_term)
  {
    int sleep_for = 10;
    for (int iters = 0; iters < retry_time * 3; iters++) {
      int nc = NumCommitted(index);
      if (nc >= num_committed_server)
        break;
      
      mssleep(sleep_for);

      if (sleep_for < 1000)
        sleep_for *= 2;
      
      if (start_term > -1) {
        for (auto &&client: clients) {
          auto res = client->call(RAFT_RPC_CHECK_LEADER);
          int current_term = std::get<1>(res.unwrap()->as<std::tuple<bool, int>>());
          if (current_term > start_term) {
            return -1;
          }
        }
      }
    }

    int nc = NumCommitted(index);
    EXPECT_GE(nc, num_committed_server) << "only " << nc << " decided for index "
                                        << index << "; wanted "
                                        << num_committed_server;
    
    return GetCommittedValue(index);
  }

  void DisableNode(int node_id)
  {
    node_network_available[node_id] = false;
    // std::cerr << "  Node " << node_id << " disabled" << std::endl;
    for (auto &&node: nodes) {
      node->set_network(node_network_available);
    }
  }

  void EnableNode(int node_id)
  {
    node_network_available[node_id] = true;
    // std::cerr << "  Node " << node_id << " enabled" << std::endl;
    for (auto &&node: nodes) {
      node->set_network(node_network_available);
    }
  }

  void SetReliable(bool flag)
  {
    for (auto &&node: nodes) {
      node->set_reliable(flag);
    }

    /* increase retry_time as the network becomes unreliable */
    retry_time = 20;
  }

  void Restart(int node_id)
  {
    DisableNode(node_id);

    clients[node_id].reset();
    nodes[node_id].reset();
    states[node_id].reset();

    nodes[node_id] = std::make_unique<RaftNode<ListStateMachine, ListCommand>>(node_id, configs);
    clients[node_id] = std::make_unique<RpcClient>(configs[node_id].ip_address, configs[node_id].port, true);
    states[node_id] = std::make_unique<ListStateMachine>();

    while (clients[node_id]->get_connection_state() != rpc::client::connection_state::connected) {
      sleep(1);
    }

    clients[node_id]->call(RAFT_RPC_START_NODE);

    EnableNode(node_id);
  }

  int RpcCount(int node_id)
  {
    if (node_id == -1) {
      int sum = 0;
      for (auto &&node: nodes) {
        sum += node->rpc_count();
      }
      
      return sum;
    } else {
      return nodes[node_id]->rpc_count();
    }
  }
};

}