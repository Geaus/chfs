//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// server.h
//
// Identification: src/include/librpc/server.h
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/config.h"
#include "rpc/server.h"

namespace chfs {

/**
 * A RPC server based on rpclib, a msgpack-rpc library.
 *
 * The server maintains a registry of function bindings that it uses to
 * dispatch calls. It also manages worker threads and connections.
 *
 * The server doesn't start listening just after the construction to allow
 * users binding any functions before running. Use the `run` function to
 * start listening on the port.
 */
class RpcServer {
public:
  /**
   * Constructs a rpc server that listens on `localhost` with the given port.
   * @param port: The port number to listen on.
   */
  explicit RpcServer(u16 port);

  /**
   * Constructs a rpc server that listens on `address:port`.
   * @param address: The address to bind to. Notice that it only works if one
   * of the network adapters control the given address.
   * @param port: The port number to listen on.
   */
  RpcServer(std::string const &address, u16 port);

  /**
   * Destructor.
   *
   * When a server is destroyed, all ongoing sessions will be closed gracefully.
   */
  ~RpcServer();

  /**
   * Starts the server loop. The server will start listening on the specified
   * address (if given) and port.
   *
   * By default, this is a blocking call, which means all the request handlers
   * are executed on the same thread that calls `run`. But non-blocking call is
   * also supported by setting param `async` to true. Async mode makes the event
   * loop start on different threads. It sets up a worker thread pool for the
   * server. All request handlers will be executed on one of these threads.
   *
   * @param async: whether the server should run on `async` mode.
   * @param port: The size of worker thread pool. It only works if `async` is
   * set to be `true`.
   */
  void run(bool async = false, std::size_t worker_threads = 1);

  /**
   * Binds a handler to a name so it becomes callable via RPC.
   *
   * This function template accepts a wide range of callables. The arguments
   * and return types of these callables should be serializable by msgpack.
   * `bind` effectively generates a suitable, light-weight compile-time
   * wrapper for the handler.
   *
   * Here're some code examples for using this function to bind handlers.
   * ```cpp
   * double divide(double a, double b) { return a / b; }
   *
   * struct subtractor {
   *   double operator()(double a, double b) { return a - b; }
   * };
   *
   * struct multiplier {
   *   double multiply(double a, double b) { return a * b; }
   * };
   *
   * int main() {
   *   subtractor s;
   *   multiplier m;
   *   auto add = [](double a, double b) { return a + b; };
   *
   *   auto server = RpcServer(8080);
   *   server.bind("add", add);
   *   server.bind("sub", s);
   *   server.bind("div", &divide);
   *   server.bind("mul", [&m](double a, double b) { return m.multiply(a, b);
   * });
   * }
   * ```
   *
   * @param name: The name of the handler. Clients need to use the name to
   * invoke calls.
   * @param handler: The handler to bind.
   *
   * @tparam F: the type of the handler.
   */
  template <typename F> void bind(std::string const &name, F handler) {
    server->bind(name, handler);
  }

  /**
   * Unregisters a handler bound to the name.
   *
   * This function removes already bound handler from RPC callable handlers.
   *
   * @param name: The name of the handler.
   */
  void unbind(std::string const &name) { server->unbind(name); }

  /**
   * Gets all the bound names.
   *
   * This function returns a list of all names which have already been bound to
   * handlers.
   */
  auto names() -> std::vector<std::string> { return server->names(); }

  /**
   * Stops the server.
   */
  void stop();

  /**
   * Closes all ongoing sessions gracefully.
   */
  void close_sessions();

private:
  std::unique_ptr<rpc::server> server;
};

} // namespace chfs