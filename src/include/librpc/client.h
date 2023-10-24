//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// client.h
//
// Identification: src/include/librpc/client.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/result.h"
#include "common/util.h"
#include "rpc/client.h"

namespace chfs {

const u32 KFailureRate = 10; // Network failure rate = 1/KFailureRate

using RpcResponse = RPCLIB_MSGPACK::object_handle;

/**
 * A client connects to rpc server. The client can call handlers
 * synchronously or asynchronously.
 *
 * The client connects to the given server asynchronically once created
 * and disconnects when destroyed.
 */
class RpcClient {
public:
  /**
   * Construct a Client.
   *
   * When a client is constructed, it initiate a connection to the server
   * asynchronically. It means that the client *might* not be ready even
   * after creating. If the connection is still not ready when one handler
   * is called, it will block until the connection is ready.
   *
   * @param addr: The address of the server to connect to. It supports both
   * IP address and hostname.
   * @param port: The port on the server to connect to.
   *
   * @param reliable: Whether the network drops packet or not. It's about
   * to simulate network failure for fault tolerance.
   */
  RpcClient(std::string const &addr, u16 port, bool reliable);

  /**
   * Destructor.
   *
   * During destruction, the connection to the server is gracefully closed.
   * This means that any outstanding reads and writes are completed first.
   */
  ~RpcClient();

  /**
   * Call a handler with the given name and args(if any).
   *
   * @param name: The name of the handler to call on the server.
   * @param args: Arguments that pass to the handler.
   *
   * @tparam Args: The types of arguments. Each type should be serializable
   * by `msgpack`.
   *
   * @return: `RpcResult` as the result of the request(if any) or error if
   * failed. Using `shared_ptr` since `std::variant` doesn't support
   * `RpcResponse`.
   */
  template <typename... Args>
  auto call(std::string const &name, Args... args)
      -> ChfsResult<std::shared_ptr<RpcResponse>> {
    // Calculate whether the request is reliable or not
    bool valid =
        reliable || (!reliable && (generator.rand(1, KFailureRate * 10)) < 10);

    if (valid) {
      // Send, wait and return
      auto res = client->call(name, args...);

      return ChfsResult<std::shared_ptr<RpcResponse>>(
          std::make_shared<RpcResponse>(res.get(), std::move(res.zone())));
    } else {
      // Judge whether we should send the request or just directly drop
      // the request as if timeout
      if (generator.rand(1, 10) < 5)
        return ChfsResult<std::shared_ptr<RpcResponse>>(ErrorType::BadResponse);
      else {
        // Send it since we don't care about the return value
        client->send(name, args...);
        return ChfsResult<std::shared_ptr<RpcResponse>>(ErrorType::RpcTimeout);
      }
    }
  };

  /**
   * Call a handler with the given name and args(if any) asynchronously.
   *
   * It doesn't wait for the response of the request.
   *
   * @param name: The name of the handler to call on the server.
   * @param args: Arguments that pass to the handler.
   *
   * @tparam Args: The types of arguments. Each type should be serializable
   * by `msgpack`.
   *
   * @return: A std::future to fetch future result(if any) or error if failed.
   * Using `shared_ptr` since `std::variant` doesn't support
   * `std::future<RpcResponse>`.
   */
  template <typename... Args>
  auto async_call(std::string const &name, Args... args)
      -> ChfsResult<std::shared_ptr<std::future<RpcResponse>>> {
    // Also check whether the req is valid or not
    bool valid =
        reliable || (!reliable && (generator.rand(1, KFailureRate * 10)) < 10);

    if (valid) {
      auto ft = client->async_call(name, args...);
      return ChfsResult<std::shared_ptr<std::future<RpcResponse>>>(
          std::make_shared<std::future<RpcResponse>>(std::move(ft)));
    } else {
      // Judge whether we should send the request or just directly drop
      // the request as if timeout
      if (generator.rand(1, 10) < 5)
        return ChfsResult<std::shared_ptr<std::future<RpcResponse>>>(
            ErrorType::BadResponse);
      else {
        // Send it since we don't care about the return value
        client->send(name, args...);
        return ChfsResult<std::shared_ptr<std::future<RpcResponse>>>(
            ErrorType::RpcTimeout);
      }
    }
  };

  /**
   * Get the current connection state.
   */
  auto get_connection_state() -> rpc::client::connection_state;

private:
  std::unique_ptr<rpc::client> client;
  bool reliable;
  RandomNumberGenerator generator;
};
} // namespace chfs