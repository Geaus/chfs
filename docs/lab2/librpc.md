# Introduction to LibRPC

LibRPC is the RPC module in chfs. It follows the server-client pattern. Server sets up function handlers and receives requests sent from client. The doc introduces how to use these two components.

## Server

**Notice**: You can refer to `src/include/librpc/server.h` and `src/librpc/server.cc` for this part.
The RPC Server maintains a registry of function bindings that it uses to dispatch RPC calls. It listens at given address and port for building connection and requests. In this lab, we've provided the initialization part so you don't need to use any part of codes in Server. But if you're interested, you can read the comments in these codes for more information.

## Client


**Notice**: You can refer to `src/include/librpc/client.h` and `src/librpc/client.cc` for this part.
The RPC Client connects to a specific RPC Server. It can invoke RPC handlers at RPC Server.
Since you might need to invoke handlers in your own implementation. Now we'll introduce some function in the class and how to use it.

Basically, the RPC Client support two ways of invocation: synchronous or asynchronous. In synchronous way, the client will wait until the request is finished. The return value of a RPC call is `RpcResponse`, which you can think is a list of bytes. And you need to call function to transform it into the thing you want. The example is below:
```cpp
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);
  auto add_res = cli->call("add", 2, 3);
  int res = add_res.unwrap()->as<int>();
```
In asynchronous way, the client will return immediately. It returns a `std::future` to get the result later. The example is below:
```cpp
  auto cli = std::make_shared<RpcClient>("127.0.0.1", port, true);
  auto add_future = cli->async_call("add", 2, 3).unwrap();
  int res = add_future->get().as<int>();
```
For more test cases and how to use it, please refer to `test/librpc/client_test.cc`.