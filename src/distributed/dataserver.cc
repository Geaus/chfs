#include "distributed/dataserver.h"
#include "common/util.h"

namespace chfs {

auto DataServer::initialize(std::string const &data_path) {
  /**
   * At first check whether the file exists or not.
   * If so, which means the distributed chfs has
   * already been initialized and can be rebuilt from
   * existing data.
   */


  bool is_initialized = is_file_exist(data_path);
  auto bm = std::shared_ptr<BlockManager>(
      new BlockManager(data_path, KDefaultBlockCnt));

    auto version_per_block = bm->block_size() / sizeof (version_t);
    auto total_version_block = bm->total_blocks() / version_per_block;

    if(bm->total_blocks() % version_per_block != 0){
        total_version_block += 1;
    }


  if (is_initialized) {
    block_allocator_ =
        std::make_shared<BlockAllocator>(bm, total_version_block, false);
  } else {
    // We need to reserve some blocks for storing the version of each block
    block_allocator_ = std::shared_ptr<BlockAllocator>(
        new BlockAllocator(bm, total_version_block, true));
  }


  // Initialize the RPC server and bind all handlers
  server_->bind("read_data", [this](block_id_t block_id, usize offset,
                                    usize len, version_t version) {
    return this->read_data(block_id, offset, len, version);
  });
  server_->bind("write_data", [this](block_id_t block_id, usize offset,
                                     std::vector<u8> &buffer) {
    return this->write_data(block_id, offset, buffer);
  });
  server_->bind("alloc_block", [this]() { return this->alloc_block(); });
  server_->bind("free_block", [this](block_id_t block_id) {
    return this->free_block(block_id);
  });

  // Launch the rpc server to listen for requests
  server_->run(true, num_worker_threads);
}

DataServer::DataServer(u16 port, const std::string &data_path)
    : server_(std::make_unique<RpcServer>(port)) {
  initialize(data_path);
}

DataServer::DataServer(std::string const &address, u16 port,
                       const std::string &data_path)
    : server_(std::make_unique<RpcServer>(address, port)) {
  initialize(data_path);
}

DataServer::~DataServer() { server_.reset(); }

// {Your code here}
auto DataServer::read_data(block_id_t block_id, usize offset, usize len,
                           version_t version) -> std::vector<u8> {
  // TODO: Implement this function.
    auto version_per_block = block_allocator_->bm->block_size() / sizeof (version_t);
    auto version_block_id = block_id / version_per_block;
    auto version_block_idx = block_id % version_per_block;

    std::vector<u8> buffer(block_allocator_->bm->block_size());
    block_allocator_->bm->read_block(version_block_id, buffer.data());
    version_t * entrys= reinterpret_cast<version_t *>(buffer.data());

//    std::cerr<<entrys[version_block_idx]<<" "<<version<<std::endl;

    if(entrys[version_block_idx] == version){

        std::vector<u8> data_buffer(block_allocator_->bm->block_size());
        block_allocator_->bm->read_block(block_id, data_buffer.data());
        std::vector<u8> result(len);
        for(usize i =0 ; i < len ; i++){
            result[i] = data_buffer[offset + i];
        }

        return result;
    }

//  UNIMPLEMENTED();

  return {};
}

// {Your code here}
auto DataServer::write_data(block_id_t block_id, usize offset,
                            std::vector<u8> &buffer) -> bool {
  // TODO: Implement this function.
  ChfsNullResult result = block_allocator_->bm->write_partial_block(block_id,buffer.data(),offset,buffer.size());

  if(result.is_ok()){
      return true;
  }
//  UNIMPLEMENTED();

  return false;
}

// {Your code here}
auto DataServer::alloc_block() -> std::pair<block_id_t, version_t> {
  // TODO: Implement this function.

    auto version_per_block = block_allocator_->bm->block_size() / sizeof (version_t);

    auto block_id =  block_allocator_->allocate();
    if( block_id.is_ok() ){

        auto version_block_id = block_id.unwrap() / version_per_block;
        auto version_block_idx = block_id.unwrap() % version_per_block;
        std::vector<u8> buffer(block_allocator_->bm->block_size());

        block_allocator_->bm->read_block(version_block_id, buffer.data());
        version_t * entrys= reinterpret_cast<version_t *>(buffer.data());
        entrys[version_block_idx] += 1;
        version_t res = entrys[version_block_idx];
        block_allocator_->bm->write_block(version_block_id, buffer.data());

        std::pair<block_id_t, version_t> result(block_id.unwrap(),res);
        return result;
    }

//  UNIMPLEMENTED();
  return {};
}

// {Your code here}
auto DataServer::free_block(block_id_t block_id) -> bool {
  // TODO: Implement this function.
  auto version_per_block = block_allocator_->bm->block_size() / sizeof (version_t);

  if( block_allocator_->deallocate(block_id).is_ok() ){

      auto version_block_id = block_id / version_per_block;
      auto version_block_idx = block_id % version_per_block;
      std::vector<u8> buffer(block_allocator_->bm->block_size());

      block_allocator_->bm->read_block(version_block_id, buffer.data());
      version_t * entrys= reinterpret_cast<version_t *>(buffer.data());
      entrys[version_block_idx] += 1;
      block_allocator_->bm->write_block(version_block_id, buffer.data());

      return true;
  }

//  UNIMPLEMENTED();

  return false;
}
} // namespace chfs