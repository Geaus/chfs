#include "distributed/metadata_server.h"
#include "common/util.h"
#include "filesystem/directory_op.h"
#include <fstream>

namespace chfs {

inline auto MetadataServer::bind_handlers() {
  server_->bind("mknode",
                [this](u8 type, inode_id_t parent, std::string const &name) {
                  return this->mknode(type, parent, name);
                });
  server_->bind("unlink", [this](inode_id_t parent, std::string const &name) {
    return this->unlink(parent, name);
  });
  server_->bind("lookup", [this](inode_id_t parent, std::string const &name) {
    return this->lookup(parent, name);
  });
  server_->bind("get_block_map",
                [this](inode_id_t id) { return this->get_block_map(id); });
  server_->bind("alloc_block",
                [this](inode_id_t id) { return this->allocate_block(id); });
  server_->bind("free_block",
                [this](inode_id_t id, block_id_t block, mac_id_t machine_id) {
                  return this->free_block(id, block, machine_id);
                });
  server_->bind("readdir", [this](inode_id_t id) { return this->readdir(id); });
  server_->bind("get_type_attr",
                [this](inode_id_t id) { return this->get_type_attr(id); });
}

inline auto MetadataServer::init_fs(const std::string &data_path) {
  /**
   * Check whether the metadata exists or not.
   * If exists, we wouldn't create one from scratch.
   */
  bool is_initialed = is_file_exist(data_path);

  auto block_manager = std::shared_ptr<BlockManager>(nullptr);
  if (is_log_enabled_) {
    block_manager =
        std::make_shared<BlockManager>(data_path, KDefaultBlockCnt, true);
  } else {
    block_manager = std::make_shared<BlockManager>(data_path, KDefaultBlockCnt);
  }

  CHFS_ASSERT(block_manager != nullptr, "Cannot create block manager.");

  if (is_initialed) {
    auto origin_res = FileOperation::create_from_raw(block_manager);
    std::cout << "Restarting..." << std::endl;
    if (origin_res.is_err()) {
      std::cerr << "Original FS is bad, please remove files manually."
                << std::endl;
      exit(1);
    }

    operation_ = origin_res.unwrap();
  } else {
    operation_ = std::make_shared<FileOperation>(block_manager,
                                                 DistributedMaxInodeSupported);
    std::cout << "We should init one new FS..." << std::endl;
    /**
     * If the filesystem on metadata server is not initialized, create
     * a root directory.
     */
    auto init_res = operation_->alloc_inode(InodeType::Directory);
    if (init_res.is_err()) {
      std::cerr << "Cannot allocate inode for root directory." << std::endl;
      exit(1);
    }

    CHFS_ASSERT(init_res.unwrap() == 1, "Bad initialization on root dir.");
  }

  running = false;
  num_data_servers =
      0; // Default no data server. Need to call `reg_server` to add.

  if (is_log_enabled_) {
    if (may_failed_)
      operation_->block_manager_->set_may_fail(true);
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled_);
  }

  bind_handlers();

  /**
   * The metadata server wouldn't start immediately after construction.
   * It should be launched after all the data servers are registered.
   */
}

MetadataServer::MetadataServer(u16 port, const std::string &data_path,
                               bool is_log_enabled, bool is_checkpoint_enabled,
                               bool may_failed)
    : is_log_enabled_(is_log_enabled), may_failed_(may_failed),
      is_checkpoint_enabled_(is_checkpoint_enabled) {
  server_ = std::make_unique<RpcServer>(port);
  init_fs(data_path);
  if (is_log_enabled_) {
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled);
  }
}

MetadataServer::MetadataServer(std::string const &address, u16 port,
                               const std::string &data_path,
                               bool is_log_enabled, bool is_checkpoint_enabled,
                               bool may_failed)
    : is_log_enabled_(is_log_enabled), may_failed_(may_failed),
      is_checkpoint_enabled_(is_checkpoint_enabled) {
  server_ = std::make_unique<RpcServer>(address, port);
  init_fs(data_path);
  if (is_log_enabled_) {
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled);
  }
}

// {Your code here}
auto MetadataServer::mknode(u8 type, inode_id_t parent, const std::string &name)
    -> inode_id_t {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"1";
    if(is_log_enabled_ ) {
        std::vector<std::shared_ptr<BlockOperation>> ops;

        auto b_m_ = operation_->block_manager_;
        auto b_a_ = operation_->block_allocator_;
        auto i_m_ = operation_->inode_manager_;

        /////////////////////////////////////////////////////////
        std::vector<u8> bitmap_buffer(b_m_->block_size());
        block_id_t new_bid = 0;
        inode_id_t new_inode = 0;

        for (uint i = 0; i < b_a_->total_bitmap_block(); i++) {
            b_m_->read_block(i + b_a_->get_bitmap_id(), bitmap_buffer.data());
            auto bitmap = Bitmap(bitmap_buffer.data(), b_m_->block_size());

            std::optional<block_id_t> res = std::nullopt;
            if (i == b_a_->total_bitmap_block() - 1) {
                res = bitmap.find_first_free_w_bound(b_a_->get_last_num());
            } else {
                res = bitmap.find_first_free();
            }
            if (res) {

                bitmap.set(res.value());
//                bm->write_block(i + this->bitmap_block_id, buffer.data());
//                ops.push_back(std::make_shared<BlockOperation>(i+b_a_->get_bitmap_id(),bitmap_buffer));

                const auto total_bits_per_block = b_m_->block_size() * KBitsPerByte;
                new_bid = i * total_bits_per_block + res.value();
                std::cout <<"inode block id: "<< new_bid << std::endl;
                //UNIMPLEMENTED();
                break;
            }
        }
        ////////////////////////////////////////////////////////
        auto iter_res = BlockIterator::create(b_m_.get(), 1 + i_m_->get_table_blocks(),
                                              i_m_->get_reserved_blocks());

        inode_id_t count = 0;
        // Find an available inode ID.
        for (auto iter = iter_res.unwrap(); iter.has_next();
             iter.next(b_m_->block_size()).unwrap(), count++) {

            auto data = iter.unsafe_get_value_ptr<u8>();
            auto bitmap = Bitmap(data, b_m_->block_size());
            auto free_idx = bitmap.find_first_free();

            if (free_idx) {
                bitmap.set(free_idx.value());
//                auto res = iter.flush_cur_block();
//                ops.push_back(std::make_shared<BlockOperation>(
//                        iter.start_block_id + this->cur_block_off / bm->block_sz,bitmap_buffer));
//
//                auto target_block_id =
//                        this->start_block_id + this->cur_block_off / bm->block_sz;
//                return this->bm->write_block(target_block_id, this->buffer.data());

                const auto total_bits_per_block = b_m_->block_size() * KBitsPerByte;
                inode_id_t raw_inode_id = count * total_bits_per_block + free_idx.value();
//                i_m_->set_table(raw_inode_id, new_bid);
                std::vector<u8> table_buffer(b_m_->block_size());

                const auto inode_per_block = b_m_->block_size() / sizeof(block_id_t);
                block_id_t inode_table_block_id = raw_inode_id / inode_per_block + 1;
                usize i = raw_inode_id % inode_per_block;

                b_m_->read_block(inode_table_block_id, table_buffer.data());
                block_id_t *entrys = reinterpret_cast<block_id_t *>(table_buffer.data());
                entrys[i] = new_bid;
                //inode table
                ops.push_back(std::make_shared<BlockOperation>(inode_table_block_id, table_buffer));

                new_inode = raw_inode_id + 1;
                std::cout <<"new inode id: "<< new_inode << std::endl;
            }

        }
        ///////////////////////////////////////////////////////////
        std::vector<u8> inode_buffer(b_m_->block_size());
        auto newNode = Inode(type == RegularFileType ? InodeType::Regular : InodeType::Directory, b_m_->block_size());
        newNode.flush_to_buffer(inode_buffer.data());
//        block_manager_->write_block(bid,buffer.data());
        //inode block
        ops.push_back(std::make_shared<BlockOperation>(new_bid, inode_buffer));
        //////////////////////////////////////////////////////////
        std::list<DirectoryEntry> list;

        read_directory(operation_.get(), parent, list);
        std::string content = dir_list_to_string(list);
        content = append_to_directory(content, name, new_inode);
        std::cout<<"data is "<<content<<std::endl;

        std::vector<u8> parent_block(b_m_->block_size());
        block_id_t parent_block_id = i_m_->get(parent).unwrap();
        b_m_->read_block(parent_block_id,parent_block.data());
        Inode *inode_p = reinterpret_cast<Inode *>(parent_block.data());

        usize old_block_num = 0;
        usize new_block_num = 0;
        u64 original_file_sz = 0;
        const auto block_sz = b_m_->block_size();

        original_file_sz = inode_p->get_size();
        old_block_num = (original_file_sz % block_sz) ? (original_file_sz / block_sz + 1) : (original_file_sz / block_sz);
        new_block_num = (content.size() % block_sz) ? (content.size() / block_sz + 1) : (content.size() / block_sz);

        if (new_block_num > old_block_num) {
            // If we need to allocate more blocks.
            for (usize idx = old_block_num; idx < new_block_num; ++idx) {

                for (uint i = 0; i < b_a_->total_bitmap_block(); i++) {
                    b_m_->read_block(i + b_a_->get_bitmap_id(), bitmap_buffer.data());
                    auto bitmap = Bitmap(bitmap_buffer.data(), b_m_->block_size());

                    std::optional<block_id_t> res = std::nullopt;
                    if (i == b_a_->total_bitmap_block() - 1) {
                        res = bitmap.find_first_free_w_bound(b_a_->get_last_num());
                    } else {
                        res = bitmap.find_first_free();
                    }
                    if (res) {

                        bitmap.set(res.value());
//                bm->write_block(i + this->bitmap_block_id, buffer.data());
//                ops.push_back(std::make_shared<BlockOperation>(i+b_a_->get_bitmap_id(),bitmap_buffer));

                        const auto total_bits_per_block = b_m_->block_size() * KBitsPerByte;
                        block_id_t parent_data_block = i * total_bits_per_block + res.value();
                        std::cout <<"new parent inode data block id: "<< parent_data_block << std::endl;

                        inode_p->set_block_direct(idx,parent_data_block);
                        //UNIMPLEMENTED();
                        break;
                    }
                }
            }
        }

        ops.push_back(std::make_shared<BlockOperation>(parent_block_id, parent_block));

        auto block_idx = 0;
        u64 write_sz = 0;
        while (write_sz < content.size()) {
            auto sz = ((content.size() - write_sz) > b_m_->block_size())
                      ? b_m_->block_size()
                      : (content.size() - write_sz);
            std::vector<u8> buffer(b_m_->block_size());
            memcpy(buffer.data(), content.data() + write_sz, sz);

            block_id_t write_block_id = 0;

            if (inode_p->is_direct_block(block_idx)) {
                write_block_id = inode_p->blocks[block_idx];
                std::cout<<"write to data block: "<< write_block_id<<std::endl;
            }

            ops.push_back(std::make_shared<BlockOperation>(write_block_id, buffer));

            write_sz += sz;
            block_idx += 1;
        }


        /////////////////////////////////////////////////////////

        commit_log->append_log(commit_log->get_log_entry_num()+1,ops);
        commit_log->commit_log(commit_log->get_log_entry_num()+1);
        auto entry_num = commit_log->get_log_entry_num();
        if(entry_num >= kMaxLogSize){
            commit_log->checkpoint();
        }

    }


    auto res = operation_->mk_helper(parent,name.data(),type == RegularFileType ? InodeType::Regular : InodeType::Directory);
    if(res.is_ok() && !may_failed_){
        return res.unwrap();
    }
    return 0;

}

// {Your code here}
auto MetadataServer::unlink(inode_id_t parent, const std::string &name)
    -> bool {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"2";

    if(is_log_enabled_){
        std::vector<std::shared_ptr<BlockOperation>> ops;

        auto b_m_ = operation_->block_manager_;
        auto b_a_ = operation_->block_allocator_;
        auto i_m_ = operation_->inode_manager_;

        std::string content;
        std::list<DirectoryEntry> list;
        auto res = operation_->lookup(parent,name.data());

        inode_id_t id = res.unwrap();
        std::cout<<"unlink inode id: "<<id<<std::endl;

        auto block_id_res = i_m_->get(id);
        if(block_id_res.is_ok()){

            block_id_t block_id = block_id_res.unwrap();

            inode_id_t raw_inode_id= id - 1;
            std::vector<u8> inode_table_buffer(b_m_->block_size());
            std::vector<u8> inode_bitmap_buffer(b_m_->block_size());

            const auto inode_per_block = b_m_->block_size() / sizeof(block_id_t);
            block_id_t inode_table_block_id= raw_inode_id / inode_per_block + 1;
            usize  i = raw_inode_id%inode_per_block;

            b_m_->read_block(inode_table_block_id, inode_table_buffer.data());
            block_id_t * entrys= reinterpret_cast<block_id_t *>(inode_table_buffer.data());
            entrys[i]=0;
//            bm->write_block(inode_table_block_id, buffer.data());

            ops.push_back(std::make_shared<BlockOperation>(inode_table_block_id, inode_table_buffer));


            const auto inode_bits_per_block = b_m_->block_size() * KBitsPerByte;
            block_id_t inode_bit_block_id=raw_inode_id/inode_bits_per_block + i_m_->get_table_blocks() + 1;
            usize  bitmap_i = raw_inode_id % inode_bits_per_block;

            b_m_->read_block(inode_bit_block_id,inode_bitmap_buffer.data());
            auto inode_bitmap = Bitmap(inode_bitmap_buffer.data(),b_m_->block_size());
            inode_bitmap.clear(bitmap_i);
//            bm->write_block(inode_bit_block_id,buffer.data());
            ops.push_back(std::make_shared<BlockOperation>(inode_bit_block_id, inode_bitmap_buffer));

            ///////////////////////////////////////////////////////////////////////////////////

            const auto total_bits_per_block = b_m_->block_size() * KBitsPerByte;
            std::vector<u8> bitmap_buffer(b_m_->block_size());

            usize bitmap_id = (block_id)/total_bits_per_block;
            usize idx= (block_id)%total_bits_per_block;

            b_m_->read_block(bitmap_id + b_a_->get_bitmap_id(), bitmap_buffer.data());
            auto bitmap = Bitmap(bitmap_buffer.data(), b_m_->block_size());

            if(bitmap.check(idx)){
                bitmap.clear(idx);
//                b_m_->write_block(id + this->bitmap_block_id, bitmap_buffer.data());
                ops.push_back(std::make_shared<BlockOperation>(bitmap_id + b_a_->get_bitmap_id(), bitmap_buffer));
            }
        }
        commit_log->append_log(commit_log->get_log_entry_num()+1,ops);
        commit_log->commit_log(commit_log->get_log_entry_num()+1);
        auto entry_num = commit_log->get_log_entry_num();
        if(entry_num >= kMaxLogSize){
            commit_log->checkpoint();
        }

    }
    auto res = operation_->unlink(parent,name.data());

    if(res.is_ok() && !may_failed_){
        return true;
    }
//  UNIMPLEMENTED();

  return false;
}

// {Your code here}
auto MetadataServer::lookup(inode_id_t parent, const std::string &name)
    -> inode_id_t {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"3";

  auto result = operation_->lookup(parent,name.data());
  if(result.is_ok()){
      return result.unwrap();
  }
//  UNIMPLEMENTED();

  return 0;
}

// {Your code here}
auto MetadataServer::get_block_map(inode_id_t id) -> std::vector<BlockInfo> {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"4";

  std::vector<BlockInfo> result;

  std::vector<u8> buffer(operation_->block_manager_->block_size());
  auto block_id_res = operation_->inode_manager_->get(id);

  if(block_id_res.is_ok()){
      block_id_t block_id = block_id_res.unwrap();
      operation_->block_manager_->read_block(block_id,buffer.data());

      Inode *inode_p = reinterpret_cast<Inode *>(buffer.data());
      for(uint i = 0; i < inode_p->get_ntuples(); i++){
          if(std::get<0>(inode_p->block_infos[i]) == 0){
              continue;
          }
          block_id_t bid = std::get<0>(inode_p->block_infos[i]);
          mac_id_t mid = std::get<1>(inode_p->block_infos[i]);
          version_t vid = std::get<2>(inode_p->block_infos[i]);

          result.push_back(std::make_tuple(bid,mid,vid));
      }
      return result;
  }

//  UNIMPLEMENTED();
  return {};
}

// {Your code here}
auto MetadataServer::allocate_block(inode_id_t id) -> BlockInfo {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"5";
  mac_id_t mac_id = generator.rand(1,num_data_servers);

//  std::cerr<<"code"<<mac_id<<std::endl;

  auto res = clients_[mac_id]->call("alloc_block");

  if(res.is_ok()){

      auto [block_id, version] = res.unwrap()->as<std::pair<block_id_t, version_t>>();

      std::vector<u8> buffer(operation_->block_manager_->block_size());
      block_id_t inode_block_id = operation_->inode_manager_->get(id).unwrap();
      operation_->block_manager_->read_block(inode_block_id,buffer.data());

      auto *inode_p = reinterpret_cast<Inode *>(buffer.data());
      for(uint i = 0; i < inode_p->get_ntuples(); i++){
          if(std::get<0>(inode_p->block_infos[i]) == 0){

              std::get<0>(inode_p->block_infos[i]) = block_id;
              std::get<1>(inode_p->block_infos[i]) = mac_id;
              std::get<2>(inode_p->block_infos[i]) = version;
              inode_p->inner_attr.size += DiskBlockSize;

              break;
          }
      }

      operation_->block_manager_->write_block(inode_block_id,buffer.data());

      return std::make_tuple(block_id,mac_id,version);
  }

//  UNIMPLEMENTED();

  return {};
}

// {Your code here}
auto MetadataServer::free_block(inode_id_t id, block_id_t block_id,
                                mac_id_t machine_id) -> bool {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"6";
  auto res = clients_[machine_id]->call("free_block",block_id);

  if(res.is_ok() && res.unwrap()->as<bool>()){

      std::vector<u8> buffer(operation_->block_manager_->block_size());
      auto inode_block_id_res = operation_->inode_manager_->get(id);

      if(inode_block_id_res.is_ok()){
          block_id_t inode_block_id = inode_block_id_res.unwrap();
          operation_->block_manager_->read_block(inode_block_id,buffer.data());

          auto *inode_p = reinterpret_cast<Inode *>(buffer.data());
          for(uint i = 0; i < inode_p->get_ntuples(); i++){
              if(std::get<0>(inode_p->block_infos[i]) == block_id && std::get<1>(inode_p->block_infos[i])== machine_id){

                  std::get<0>(inode_p->block_infos[i]) = 0;
                  std::get<1>(inode_p->block_infos[i]) = 0;
                  std::get<2>(inode_p->block_infos[i]) = 0;
                  inode_p->inner_attr.size -= DiskBlockSize;

                  break;
              }
          }
          operation_->block_manager_->write_block(inode_block_id,buffer.data());
          return true;
      }
  }

//  UNIMPLEMENTED();

  return false;
}

// {Your code here}
auto MetadataServer::readdir(inode_id_t node)
    -> std::vector<std::pair<std::string, inode_id_t>> {
  // TODO: Implement this function.
  std::lock_guard<std::mutex> lock(global_lock);
//  std::cerr<<"7";

  std::vector<std::pair<std::string, inode_id_t>> result;

  std::list<DirectoryEntry> list;
  read_directory(operation_.get(),node,list);

    for (auto it = list.begin(); it != list.end(); ++it) {
        result.push_back(std::make_pair(it->name,it->id));
    }

    return result;
//  UNIMPLEMENTED();

  return {};
}

// {Your code here}
auto MetadataServer::get_type_attr(inode_id_t id)
    -> std::tuple<u64, u64, u64, u64, u8> {
  // TODO: Implement this function.
    std::lock_guard<std::mutex> lock(global_lock);
//    std::cerr<<"7";

    std::vector<u8> buffer(operation_->block_manager_->block_size());
    auto inode_block_id_res = operation_->inode_manager_->get(id);
    if(inode_block_id_res.is_ok()){

        block_id_t inode_block_id = inode_block_id_res.unwrap();
        operation_->block_manager_->read_block(inode_block_id,buffer.data());

        auto *inode_p = reinterpret_cast<Inode *>(buffer.data());
        auto type_res = inode_p->get_type();
        auto attr_res = inode_p->get_attr();

        u64 atime = attr_res.atime;
        u64 mtime = attr_res.mtime;
        u64 ctime = attr_res.ctime;
        u64 size = attr_res.size;
        u8  type = (type_res == InodeType::Regular) ? RegularFileType : DirectoryType;

        return std::make_tuple(atime,mtime,ctime,size,type);
    }

//  UNIMPLEMENTED();

  return {};
}

auto MetadataServer::reg_server(const std::string &address, u16 port,
                                bool reliable) -> bool {
  num_data_servers += 1;
  auto cli = std::make_shared<RpcClient>(address, port, reliable);
  clients_.insert(std::make_pair(num_data_servers, cli));

  return true;
}

auto MetadataServer::run() -> bool {
  if (running)
    return false;

  // Currently we only support async start
  server_->run(true, num_worker_threads);
  running = true;
  return true;
}

} // namespace chfs