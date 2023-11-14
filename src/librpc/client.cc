#include "librpc/client.h"
#include "common/error_code.h"

namespace chfs {

RpcClient::RpcClient(std::string const &addr, u16 port, bool reliable)
    : reliable(reliable), rpc_count(0) {
  client.reset(new rpc::client(addr, port));
}

RpcClient::~RpcClient() { client.reset(); }

auto RpcClient::get_connection_state() -> rpc::client::connection_state {
  return client->get_connection_state();
}

auto RpcClient::count() -> int {
  return rpc_count.load();
}

auto RpcClient::set_reliable(bool r) -> bool {
  reliable = r;
  return r;
}

} // namespace chfs
