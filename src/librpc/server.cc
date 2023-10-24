#include "librpc/server.h"

namespace chfs {

RpcServer::RpcServer(u16 port) { server = std::make_unique<rpc::server>(port); }

RpcServer::RpcServer(std::string const &address, u16 port) {
  server = std::make_unique<rpc::server>(address, port);
}

RpcServer::~RpcServer() {
  // Delete the `server` pointer
  server.reset();
}

void RpcServer::run(bool async, std::size_t worker_threads) {
  async ? server->async_run(worker_threads) : server->run();
}

void RpcServer::stop() { server->stop(); }

void RpcServer::close_sessions() { server->close_sessions(); }

} // namespace chfs