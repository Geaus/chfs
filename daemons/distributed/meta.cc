#include "distributed/metadata_server.h"
#include "./consts.h"
#include <thread>
#include <chrono>

auto main(int argc, char **argv) -> int {
  using namespace chfs;

  auto meta_srv = std::make_shared<MetadataServer>("127.0.0.1", kMetadataServerPort, kMetaBlockPath);
  for (auto i = 0; i < kDataServerNum; ++i)
    meta_srv->reg_server("127.0.0.1", kDataServerPorts[i], true);

  meta_srv->run();

  // Sleep here forever
  while (true) {
     std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}