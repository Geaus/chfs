#include "distributed/dataserver.h"
#include "./consts.h"
#include <thread>
#include <chrono>

auto main(int argc, char **argv) -> int {
  using namespace chfs;

  auto data_srvs = std::vector<std::shared_ptr<DataServer>>();
  for (auto i = 0; i < kDataServerNum; ++i)
    data_srvs.push_back(std::make_shared<DataServer>("127.0.0.1", kDataServerPorts[i], kDataBlockPath[i]));

  // Sleep here forever
  while (true) {
     std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}